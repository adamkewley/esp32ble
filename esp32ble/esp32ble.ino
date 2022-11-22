#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <cstddef>
#include <cstdint>

// macros
#define AK_TOKENPASTE(x, y) x##y
#define AK_TOKENPASTE2(x, y) AK_TOKENPASTE(x, y)
#define AK_DEBUG

// perf stuff
namespace ak
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

    size_t size() const
    {
      return sizeof(PerfCounters)/sizeof(PerfCounter);
    }

    const PerfCounter& operator[](size_t i) const
    {
      static_assert(sizeof(PerfCounters) % sizeof(PerfCounter) == 0, "");
      static_assert(alignof(PerfCounters) == alignof(PerfCounter), "");
      return reinterpret_cast<const PerfCounter*>(this)[i];
    }

    PerfCounter analogRead{"analogRead"};
    PerfCounter allReads{"allReads"};

  } g_PerfCounters;

  // update an existing counter with a new measurement
  void SubmitPerfMeasurement(PerfCounter& counter, uint64_t start, uint64_t end)
  {
    counter.accumulator += end - start;
    counter.counter++;
  }

  // print a counter to the serial output
  void PrintPerfCounterToSerial(const PerfCounter& counter)
  {
    if (counter.counter <= 0)
    {
      return;  // no measurements
    }
    const uint64_t avg = counter.accumulator / counter.counter;
    Serial.printf("    %s: num calls = %u avg. micros = %u\n", counter.label, counter.counter, avg);
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
    BlockScopedPerfMeasurement(PerfCounter& target) :
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
}

#ifdef AK_DEBUG
#define AK_PERF(counter) ak::BlockScopedPerfMeasurement AK_TOKENPASTE2(timer_, __LINE__)(counter)
#else
#define AK_PERF(counter)
#endif

// instrumented functions
namespace ak
{
  uint16_t analogRead(uint8_t pin)
  {
    AK_PERF(g_PerfCounters.analogRead);
    return ::analogRead(pin);  // call base version
  }
}

void setup()
{
  Serial.begin(9600);
}

void loop()
{
  {
    AK_PERF(ak::g_PerfCounters.allReads);
    const int a1Val = ak::analogRead(A1);
    const int a2Val = ak::analogRead(A2);
    const int a3Val = ak::analogRead(A3);
  }
  
  ak::PrintAllPerfCountersToSerial();
}
