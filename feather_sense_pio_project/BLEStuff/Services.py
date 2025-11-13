# ble_list_and_notify_robust.py
import asyncio
from bleak import BleakScanner, BleakClient

# Optional: set a specific characteristic UUID you expect (or leave None to auto-pick)
CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  # e.g. "6e400003-b5a3-f393-e0a9-e50e24dcca9e" Set to None for the first notify char to be selected

def get_rssi(device):
    if hasattr(device, "rssi"):
        return device.rssi
    meta = getattr(device, "metadata", None)
    if isinstance(meta, dict):
        return meta.get("rssi", "N/A")
    return "N/A"

def notification_handler(sender, data):
    print(f"[NOTIFY] {sender}: {data!r}")

async def fetch_services(client):
    """
    Return a Bleak Services collection in a way compatible with multiple bleak versions.
    """
    # Prefer awaitable get_services() if available
    get_srv = getattr(client, "get_services", None)
    if callable(get_srv):
        try:
            services = await client.get_services()
            return services
        except Exception:
            # Fall back to property if coroutine call fails
            pass

    # Fallback: some bleak versions populate client.services property after connect()
    if hasattr(client, "services"):
        services = getattr(client, "services")
        if services:
            return services

    # Last resort: some bleak versions require calling client.get_services_sync or similar (rare)
    raise RuntimeError(
        "Unable to obtain services from BleakClient. "
        "This Bleak installation does not expose get_services() nor a populated client.services property. "
        "Please check bleak version or provide the services via a different API."
    )

async def main():
    print("Scanning for 5 seconds...")
    devices = await BleakScanner.discover(timeout=5.0)

    if not devices:
        print("No BLE devices found. Make sure your Feather is advertising.")
        return

    for i, d in enumerate(devices):
        print(f"{i}: Name='{d.name}'  Address='{d.address}'  RSSI={get_rssi(d)}")

    sel = input("Enter device index or address: ").strip()
    if sel.isdigit():
        idx = int(sel)
        if idx < 0 or idx >= len(devices):
            print("Index out of range.")
            return
        addr = devices[idx].address
    else:
        addr = sel

    client = BleakClient(addr)
    try:
        print(f"Connecting to {addr} ...")
        await client.connect(timeout=10.0)
    except Exception as e:
        print("Failed to connect:", e)
        return

    try:
        # Connection state: in 1.1.1 client.is_connected is a boolean property
        connected = getattr(client, "is_connected")
        print("Connected:", bool(connected))

        if not connected:
            print("Client not connected after connect(). Exiting.")
            return

        # Fetch services in a version-robust way
        services = await fetch_services(client)
        print("\nDiscovered services and characteristics:")
        notify_chars = []
        for service in services:
            print(f"\nService: {service.uuid}")
            for char in service.characteristics:
                props = char.properties
                print(f"  Char: {char.uuid}")
                print(f"    Properties: {props}")
                for desc in char.descriptors:
                    print(f"    Descriptor: {desc.uuid} (handle {getattr(desc, 'handle', 'N/A')})")
                if "notify" in props or "indicate" in props:
                    notify_chars.append(char.uuid)

        chosen_uuid = CHAR_UUID or (notify_chars[0] if notify_chars else None)
        if chosen_uuid is None:
            print("\nNo characteristic with 'notify'/'indicate' found.")
            print("If you expect notifications, ensure the peripheral creates the characteristic with notify enabled.")
            return

        print(f"\nSubscribing to notifications on: {chosen_uuid}")
        try:
            await client.start_notify(chosen_uuid, notification_handler)
        except Exception as e:
            print("start_notify() failed:", repr(e))
            if "NotSupported" in repr(e) or "NotSupported" in str(e):
                print("The peripheral likely does not support notifications on that characteristic.")
            return

        print("Listening for notifications. Press Ctrl-C to quit.")
        while True:
            await asyncio.sleep(1)

    finally:
        try:
            if getattr(client, "is_connected", False):
                try:
                    # attempt to stop notify if we subscribed earlier
                    if 'chosen_uuid' in locals() and chosen_uuid:
                        await client.stop_notify(chosen_uuid)
                except Exception:
                    pass
                await client.disconnect()
        except Exception:
            pass

if __name__ == "__main__":
    asyncio.run(main())
