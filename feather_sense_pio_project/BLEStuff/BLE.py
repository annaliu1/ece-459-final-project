# pip install bleak
import asyncio
from bleak import BleakClient, BleakScanner

TARGET_NAME = "AnnaBLETester"   # or use address
CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # example custom characteristic UUID

async def notification_handler(sender, data: bytearray):
    # called when MCU sends a notification
    print(f"Notification from {sender}: {data.hex()}")  # parse framing here

async def run():
    devices = await BleakScanner.discover()
    addr = None
    for d in devices:
        if d.name == TARGET_NAME:
            addr = d.address
            break
    if not addr:
        print("Device not found")
        return

    async with BleakClient(addr) as client:
        print("Connected:", client.is_connected)
        await client.start_notify(CHAR_UUID, notification_handler)
        print("Notifications enabled — waiting for data (Ctrl+C to quit)")
        while True:
            await asyncio.sleep(1)

if __name__ == "__main__":
    asyncio.run(run())
