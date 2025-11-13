from bleak import BleakScanner
import asyncio

async def main():
    devices = await BleakScanner.discover(timeout=5.0)
    for d in devices:
        print(d)  # Shows name + address + RSSI

asyncio.run(main())
