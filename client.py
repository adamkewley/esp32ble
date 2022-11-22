import asyncio

import bleak

_SERVICE_UUID = "c184fdd9-7a6a-400d-ae35-8158d2efb090"
_CHARACTERISTIC_UUID = "1f20a61f-e506-488d-bf94-393ca4cdcb28"

async def ascan_for_all_devices():
    async with bleak.BleakScanner() as scanner:
        return await scanner.discover()
    
async def ais_esp32(device : bleak.BLEDevice) -> bool:
    try:
        async with bleak.BleakClient(device) as client:
            services = await client.services
            if services.get_service(_SERVICE_UUID):
                return True
    except:
        return False
    return False

async def ascan_for_esp32s():
    all_devices = await ascan_for_all_devices()
    return [d for d in all_devices if await ais_esp32(d)]

async def amain():
    esp_32s = await ascan_for_esp32s()
    for esp32 in esp_32s:
        print(esp_32s)

def main():
    loop = asyncio.new_event_loop()
    return loop.run_until_complete(amain())

if __name__ == "__main__":
    main()
