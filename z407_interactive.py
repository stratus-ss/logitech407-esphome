import asyncio
from bleak import BleakScanner, BleakClient, BleakGATTCharacteristic

SERVICE_UUID  = "0000fdc2-0000-1000-8000-00805f9b34fb"
COMMAND_UUID  = "c2e758b9-0e78-41e0-b0cb-98a593193fc5"
RESPONSE_UUID = "b84ac9c6-29c5-46d4-bba1-9d534784330f"


class Z407Remote:
    def __init__(self, address: str):
        self.address = address
        self.client = BleakClient(address)
        self.connected = False

    async def connect(self):
        print(f"Connecting to {self.address} ...")
        await self.client.connect()
        await self.client.start_notify(RESPONSE_UUID, self._receive_data)
        await self._send_command("8405")  # handshake initiate

    async def disconnect(self):
        if self.client.is_connected:
            await self.client.disconnect()

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.disconnect()

    async def _receive_data(self, sender: BleakGATTCharacteristic, data: bytearray):
        print(f"[RX] {bytes(data).hex()}")
        if data == b"\xd4\x05\x01":
            await self._send_command("8400")
        elif data == b"\xd4\x00\x01":
            self.connected = True
            print("Handshake complete; ready for commands.")

    async def _send_command(self, command: str):
        payload = bytes.fromhex(command)
        print(f"[TX] {command}")
        await self.client.write_gatt_char(COMMAND_UUID, payload, response=False)

    # High-level helpers
    async def bass_up(self):          await self._send_command("8000")
    async def bass_down(self):        await self._send_command("8001")
    async def volume_up(self):        await self._send_command("8002")
    async def volume_down(self):      await self._send_command("8003")
    async def play_pause(self):       await self._send_command("8004")
    async def next_track(self):       await self._send_command("8005")
    async def previous_track(self):   await self._send_command("8006")
    async def input_bluetooth(self):  await self._send_command("8101")
    async def input_aux(self):        await self._send_command("8102")
    async def input_usb(self):        await self._send_command("8103")
    async def bluetooth_pair(self):   await self._send_command("8200")
    async def factory_reset(self):    await self._send_command("8300")
    async def sound_1(self):          await self._send_command("8501")
    async def sound_2(self):          await self._send_command("8502")
    async def sound_3(self):          await self._send_command("8503")

    @staticmethod
    async def discover_one(timeout=8):
        print(f"Scanning for Z407 control service for {timeout}s "
              "(puck battery out; no other control client).")
        devices = await BleakScanner.discover(timeout=timeout, service_uuids=[SERVICE_UUID])
        for d in devices:
            print(f"Found: {d.name} @ {d.address}")
            return Z407Remote(d.address)
        print("No Z407 control endpoint found.")
        return None


async def repl():
    remote = await Z407Remote.discover_one()
    if not remote:
        return

    async with remote:
        await asyncio.sleep(1.0)

        print(
            "Z407 REPL commands:\n"
            "  vu, vd, bu, bd, pp, nx, pv, bt, aux, usb,\n"
            "  pair, freset, s1, s2, s3, raw <hex>, quit"
        )

        while True:
            cmd = input("> ").strip().lower()
            if cmd in ("q", "quit", "exit"):
                break
            elif cmd == "vu":
                await remote.volume_up()
            elif cmd == "vd":
                await remote.volume_down()
            elif cmd == "bu":
                await remote.bass_up()
            elif cmd == "bd":
                await remote.bass_down()
            elif cmd == "pp":
                await remote.play_pause()
            elif cmd == "nx":
                await remote.next_track()
            elif cmd == "pv":
                await remote.previous_track()
            elif cmd == "bt":
                await remote.input_bluetooth()
            elif cmd == "aux":
                await remote.input_aux()
            elif cmd == "usb":
                await remote.input_usb()
            elif cmd == "pair":
                await remote.bluetooth_pair()
            elif cmd == "freset":
                await remote.factory_reset()
            elif cmd == "s1":
                await remote.sound_1()
            elif cmd == "s2":
                await remote.sound_2()
            elif cmd == "s3":
                await remote.sound_3()
            elif cmd.startswith("raw "):
                hex_cmd = cmd.split(" ", 1)[1].strip()
                await remote._send_command(hex_cmd)
            else:
                print("Unknown command.")


if __name__ == "__main__":
    asyncio.run(repl())
