#include <NimBLEDevice.h>

#include <cstddef>
#include <cstdint>
#include <inttypes.h>

// SECTION: top-level macros
//
// edit these to change device behavior

#define MEG_DEBUG
#define MEG_BLE_LONG_NAME "MegDevice"
#define MEG_BLE_CONFIGURATION_SERVICE_UUID "c184fdd9-7a6a-400d-ae35-8158d2efb090"
#define MEG_BLE_DEVICE_INFO_CHARACTERISTIC_UUID "1f20a61f-e506-488d-bf94-393ca4cdcb28"
#define MEG_SERIAL_BAUD_RATE 9600


// SECTION: utility macros
//
// used by other parts of the codebase to do handy stuff
#define MEG_TOKENPASTE(x, y) x##y
#define MEG_TOKENPASTE2(x, y) MEG_TOKENPASTE(x, y)


// SECTION: perf
//
// utility code for taking runtime performance measurements
namespace meg
{
  // a single performance counter
  struct PerfCounter final {

    explicit PerfCounter(char const* label_) :
      label{label_}
    {
    }
    
    char const* label;
    uint64_t accumulator = 0;
    uint32_t counter = 0;
  };

  // all globally-accessible performance counters
  struct PerfCounters final {

    // returns the number of counters held within this struct
    size_t size() const
    {
      return sizeof(PerfCounters)/sizeof(PerfCounter);
    }

    // returns a reference to the `i`th counter
    const PerfCounter& operator[](size_t i) const
    {
      static_assert(sizeof(PerfCounters) % sizeof(PerfCounter) == 0, "PerfCounters should only contain PerfCounter instances");
      static_assert(alignof(PerfCounters) == alignof(PerfCounter), "PerfCounters should only contain PerfCounter instances");
      return reinterpret_cast<const PerfCounter*>(this)[i];
    }

    PerfCounter allMeasurements{"allMeasurements"};
    PerfCounter singleMeasurement{"singleMeasurement"};
    PerfCounter ble{"ble"};

  } g_PerfCounters;

  // update an existing counter with a new measurement
  void SubmitPerfMeasurement(PerfCounter& counter, uint64_t start, uint64_t end)
  {
    counter.accumulator += end - start;
    counter.counter += 1;
  }

  // print a counter to the serial output
  void PrintPerfCounterToSerial(const PerfCounter& counter)
  {
    if (counter.counter <= 0)
    {
      return;  // no measurements
    }
    const uint64_t avgMicros = counter.accumulator / counter.counter;
    const uint64_t hz = 1000000 / avgMicros;
    Serial.printf("    %s: avg. micros = %"PRIu64", hz = %"PRIu64"\n", counter.label, avgMicros, hz);
  }

  // prints all performance counters
  void PrintAllPerfCountersToSerial()
  {
    Serial.print("perf counters:\n");
    for (size_t i = 0; i < g_PerfCounters.size(); ++i)
    {
      PrintPerfCounterToSerial(g_PerfCounters[i]);
    }
  }

  // automatically measures how long the remainder of a code block lasts and update
  // the provided counter with that measurement
  class BlockScopedPerfMeasurement final {
  public:
    explicit BlockScopedPerfMeasurement(PerfCounter& target) :
      m_Target{target},
      m_Start{micros()}
    {
    }
    ~BlockScopedPerfMeasurement()
    {
      const uint64_t end = micros();
      SubmitPerfMeasurement(m_Target, m_Start, end);
    }
  private:
    PerfCounter& m_Target;
    uint64_t m_Start;
  };

  // define MEG_PERF as a macro that can be enabled/disabled based on other defines
  #ifdef MEG_DEBUG
    #define MEG_PERF(counter) meg::BlockScopedPerfMeasurement MEG_TOKENPASTE2(timer_, __LINE__)(counter)
  #else
    #define MEG_PERF(counter)
  #endif
}


// SECTION: 64-bit clock
//
// monotonically increasing 64-bit clock with no rollover
namespace meg
{
  static uint32_t g_ClockLastMicros = 0;
  static uint64_t g_ClockAccumulator = 0;

  uint64_t GetClockValue()
  {
    const uint32_t cur = micros();
    g_ClockAccumulator += cur - g_ClockLastMicros;
    g_ClockLastMicros = cur;
    return g_ClockAccumulator;
  }
}

// SECTION: data measurement
//
// code related to actually taking+storing measurement data
namespace meg
{
  struct DataMeasurement final {

    DataMeasurement() = default;
    
    explicit DataMeasurement(uint16_t value_) :
      time{GetClockValue()},
      value{value_}
    { 
    }
    uint64_t time = 0;  // senteniel: default ctor should mean "not measured"
    uint16_t value = 0;
  };

  DataMeasurement ReadMeasurement(uint8_t pin)
  {
    MEG_PERF(g_PerfCounters.singleMeasurement);
    return DataMeasurement{analogRead(pin)};
  }
}


// SECTION: bluetooth/BLE client
//
// utility classes/functions for maintaining the device's BLE API
namespace meg
{
  struct BLEDataMessage final {

    BLEDataMessage() = default;

    BLEDataMessage(
      const DataMeasurement& m1,
      const DataMeasurement& m2,
      const DataMeasurement& m3) :

      v1{m1.value},
      v2{m2.value},
      v3{m3.value}    
    {
    }

    uint16_t v1 = 0;
    uint16_t v2 = 0;
    uint16_t v3 = 0;
  };

  static_assert(sizeof(BLEDataMessage) == 3*sizeof(uint16_t), "someone has screwed with the message");

  class BLEApi final {
  public:
    BLEApi()
    {
      #ifdef MEG_DEBUG
        Serial.println("begin: initializing BLE device");
      #endif

      // init top-level device stuff + server
      BLEDevice::init(MEG_BLE_LONG_NAME);
      m_Server = BLEDevice::createServer();

      // configurate advertising
      //
      // note: it needs further configuration from each service before being turned on
      BLEAdvertising& advertising = *BLEDevice::getAdvertising();
      {
        advertising.setScanResponse(true);
        advertising.setMinPreferred(0x06);  // functions that help with iPhone connections issue
        advertising.setMinPreferred(0x12);
      }

      // init configuration service
      //
      // it's a top-level service that provides device information and lets the
      // caller control the overall device
      {
        m_ConfigurationService = m_Server->createService(MEG_BLE_CONFIGURATION_SERVICE_UUID);
        m_DeviceInfoCharacteristic = m_ConfigurationService->createCharacteristic(
          MEG_BLE_DEVICE_INFO_CHARACTERISTIC_UUID,
          NIMBLE_PROPERTY::READ
        );
        m_DeviceInfoCharacteristic->setValue(BLEDataMessage{});
        m_ConfigurationService->start();
        advertising.addServiceUUID(MEG_BLE_CONFIGURATION_SERVICE_UUID);
      }

      // done! start advertising the server to clients
      BLEDevice::startAdvertising();

      #ifdef MEG_DEBUG
        Serial.println("end  : initializing BLE device");
      #endif
    }

    void setMeasurementMessage(const BLEDataMessage& m)
    {
      m_DeviceInfoCharacteristic->setValue(m);
    }

    BLEApi(const BLEApi&) = delete;
    BLEApi(BLEApi&&) noexcept = delete;
    BLEApi& operator=(const BLEApi&) = delete;
    BLEApi& operator=(BLEApi&&) noexcept = delete;

    ~BLEApi()
    {
      BLEDevice::deinit();
    }
  private:
    BLEServer* m_Server = nullptr;
    BLEService* m_ConfigurationService = nullptr;
    BLECharacteristic* m_DeviceInfoCharacteristic = nullptr;
  };
}


// SECTION: top-level ESP32 code (main/setup/loop)

void setup()
{
  Serial.begin(MEG_SERIAL_BAUD_RATE);
  meg::BLEApi bleApi;

  while (true)
  {
    meg::DataMeasurement m1;
    meg::DataMeasurement m2;
    meg::DataMeasurement m3;

    // take emg measurements
    {
      MEG_PERF(meg::g_PerfCounters.allMeasurements);
      m1 = meg::ReadMeasurement(A1);
      m2 = meg::ReadMeasurement(A2);
      m3 = meg::ReadMeasurement(A3);
    }

    // update BLE characteristics
    {
      MEG_PERF(meg::g_PerfCounters.ble);
      meg::BLEDataMessage message{m1, m2, m3};
      bleApi.setMeasurementMessage(message);
    }

    // debug stuff
    meg::PrintAllPerfCountersToSerial();
    delay(1000);
  }
}

void loop()
{
  // don't use `loop`: we stack-allocate data in `setup`, rather than using globals
}
