#!/usr/bin/env python3
"""
EcoFlow BLADE Movement Replicator

This script replicates the exact movement sequence discovered in the btsnoop analysis.
It performs minimal initialization and then executes the precise movements that
were found to work in the handshake sequence.
"""

import asyncio
from bleak import BleakClient, BleakScanner
import time

class EcoFlowBladeMovementReplicator:
    def __init__(self):
        self.WRITE_CHAR_UUID = "ABF1"
        self.NOTIFY_CHAR_UUID = "ABF2"

        # Authentication command
        self.AUTH_COMMAND = bytes.fromhex("aa020100a00d000000000000214545260110ed")

        # Essential initialization sequence (steps 1-6, non-movement)
        self.INIT_SEQUENCE = [
            bytes.fromhex("aa020000b50d00000000000021353589c584"),
            bytes.fromhex("aa0208001d0d000000000000214545282800000000000000a6f2"),
            bytes.fromhex("aa021e00340d00000000000021454520fa190068747470733a2f2f6170692d612e65636f666c6f772e636f6d0500e670"),
            bytes.fromhex("aa0208001d0d0000000000002145453333000000010a0101b913"),
            bytes.fromhex("aa0208001d0d000000000000214545333300000001065553c62d"),
            bytes.fromhex("aa020100a00d000000000000214545110106dd"),
        ]

        # Exact movement sequence that made the robot move (steps 7-12)
        # CRITICAL DISCOVERY: These are NOT modular commands!
        # Each command is a complete sequence context, not just coordinates.
        # The "stop" command only works in position 2 - it's not a generic (0,0).
        # Moving commands around breaks the sequence - they must be used exactly as captured.
        self.MOVEMENT_SEQUENCE = [
            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000c9cff00008c07"),
                "description": "Backward (X=-100, Y=0)",
                "x": -100, "y": 0
            },
            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000c000000009267"),
                "description": "Stop (X=0, Y=0)",
                "x": 0, "y": 0
            },
            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000caeff11008eef"),
                "description": "Backward + Right Turn (X=-82, Y=17)",
                "x": -82, "y": 17
            },
            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000c5300100088e3"),
                "description": "Forward + Right Turn (X=83, Y=16)",
                "x": 83, "y": 16
            },

            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000c5600feff83cf"),
                "description": "Forward + Left Turn (X=86, Y=-2)",
                "x": 86, "y": -2
            },
            {
                "command": bytes.fromhex("aa020900080d000000000000214545010c0000000c4400140089a7"),
                "description": "Forward + Right Turn (X=68, Y=20)",
                "x": 68, "y": 20
            }
        ]

        self.client = None
        self.responses = []

    def notification_handler(self, sender, data):
        """Handle device responses with keep-alive mechanism"""
        timestamp = time.strftime("%H:%M:%S")
        hex_data = data.hex()
        print(f"📥 [{timestamp}] Response: {hex_data[:40]}...")
        self.responses.append((timestamp, hex_data))

        # CRITICAL: Keep-alive mechanism from ESP32 analysis
        # Reply to packets with dest=0x21 to maintain session
        if len(data) >= 18 and data[0:2] == bytes([0xAA, 0x02]):
            try:
                # Check if this is a packet that requires reply (dest=0x21)
                if len(data) >= 16 and data[13] == 0x21:  # dest field at offset 13
                    # Send keep-alive reply to maintain connection
                    asyncio.create_task(self.send_keep_alive_reply(data))
            except Exception as e:
                print(f"⚠️  Keep-alive reply error: {e}")

    async def send_keep_alive_reply(self, original_packet):
        """Send keep-alive reply to maintain session (from ESP32 analysis)"""
        try:
            if len(original_packet) >= 18:
                # Create reply packet by swapping src/dest
                reply = bytearray(original_packet)
                if len(reply) >= 16:
                    # Swap src (offset 12) and dest (offset 13)
                    reply[12], reply[13] = reply[13], reply[12]

                    await asyncio.sleep(0.1)  # Small delay
                    await self.client.write_gatt_char(self.WRITE_CHAR_UUID, bytes(reply))
                    print(f"🔄 Sent keep-alive reply")
        except Exception as e:
            print(f"❌ Keep-alive reply failed: {e}")

    async def find_and_connect(self):
        """Find and connect to BLADE device"""
        print("🔍 Scanning for EcoFlow BLADE...")

        devices = await BleakScanner.discover(timeout=8.0)
        blade_device = None

        for device in devices:
            if device.name and "EF_MOW" in device.name:
                blade_device = device
                print(f"✅ Found: {device.name} ({device.address})")
                break

        if not blade_device:
            print("❌ No BLADE device found")
            return False

        try:
            print("🔗 Connecting...")
            self.client = BleakClient(blade_device.address)
            await self.client.connect()

            if self.client.is_connected:
                print("✅ Connected!")

                # Enable notifications
                await self.client.start_notify(self.NOTIFY_CHAR_UUID, self.notification_handler)
                print("🔔 Notifications enabled")

                return True
            else:
                return False

        except Exception as e:
            print(f"❌ Connection error: {e}")
            return False

    async def verify_connection(self) -> bool:
        """Verify connection is still active and reconnect if needed"""
        if not self.client or not self.client.is_connected:
            print("⚠️  Connection lost, attempting to reconnect...")

            # Try to reconnect
            try:
                if self.client:
                    await self.client.disconnect()

                # Quick reconnection
                devices = await BleakScanner.discover(timeout=5.0)
                blade_device = None

                for device in devices:
                    if device.name and "EF_MOW" in device.name:
                        blade_device = device
                        break

                if not blade_device:
                    print("❌ Device not found for reconnection")
                    return False

                print("🔄 Reconnecting...")
                self.client = BleakClient(blade_device.address)
                await self.client.connect()

                if self.client.is_connected:
                    # Re-enable notifications
                    await self.client.start_notify(self.NOTIFY_CHAR_UUID, self.notification_handler)

                    # CRITICAL: Re-initialize device after reconnection
                    print("🔄 Re-initializing after reconnection...")
                    await self.initialize_device()

                    print("✅ Reconnected and re-initialized successfully!")
                    return True
                else:
                    print("❌ Reconnection failed")
                    return False

            except Exception as e:
                print(f"❌ Reconnection error: {e}")
                return False

        return True

    async def send_command(self, command: bytes, description: str, wait_time: float) -> bool:
        """Send command with specified timing and connection verification"""
        try:
            # Verify connection before each command
            if not await self.verify_connection():
                return False

            print(f"📤 {description}")
            print(f"   Command: {command.hex()}")

            response_count_before = len(self.responses)
            await self.client.write_gatt_char(self.WRITE_CHAR_UUID, command)

            # Wait for response with keep-alive monitoring
            start_time = time.time()
            while time.time() - start_time < wait_time:
                await asyncio.sleep(0.1)

                # Check if we got disconnected during wait
                if not self.client.is_connected:
                    print("❌ Connection lost during command")
                    return False

            new_responses = len(self.responses) - response_count_before
            if new_responses > 0:
                print(f"   📥 Got {new_responses} response(s)")

            return True

        except Exception as e:
            print(f"   ❌ Failed: {e}")
            # Try to reconnect on error
            await self.verify_connection()
            return False

    async def initialize_device(self) -> bool:
        """Perform essential initialization"""
        print("\n🚀 Device Initialization")
        print("=" * 30)

        # Step 1: Authentication (needs longer wait)
        print("1️⃣ Authentication...")
        success = await self.send_command(
            self.AUTH_COMMAND,
            "Sending authentication",
            2.0  # Long wait for auth
        )

        if not success:
            return False

        # Step 2: Essential setup commands (optimal timing: 0.4s)
        print(f"\n2️⃣ Setup Commands ({len(self.INIT_SEQUENCE)} steps)...")

        for i, command in enumerate(self.INIT_SEQUENCE):
            success = await self.send_command(
                command,
                f"Setup step {i+1}/{len(self.INIT_SEQUENCE)}",
                0.2  # Optimal timing confirmed by user testing
            )

            if not success:
                print(f"❌ Setup failed at step {i+1}")
                return False

        print("✅ Initialization complete!")
        return True

    async def execute_movement_sequence(self) -> bool:
        """Execute the exact movement sequence that worked with phase separation"""
        print(f"\n🎮 Movement Sequence ({len(self.MOVEMENT_SEQUENCE)} movements)")
        print("=" * 40)
        print("🤖 Watch your EcoFlow BLADE for movement!")

        # Phase 1: Backward movements (steps 1-3)
        # print("\n📍 Phase 1: Backward Movement Sequence")
        # for i in range(3):  # First 3 movements are backward
        #     movement = self.MOVEMENT_SEQUENCE[i]
        #     print(f"\n[{i+1}/3] {movement['description']}")

        #     success = await self.send_command(
        #         movement["command"],
        #         f"Coordinates: X={movement['x']}, Y={movement['y']}",
        #         0.2  # Optimal timing confirmed by user testing
        #     )

        #     if not success:
        #         print(f"❌ Backward movement failed at step {i+1}")
        #         return False

        #     await asyncio.sleep(0.5)  # Brief pause between movements

        # print("\n✅ Backward sequence complete!")

        # Phase 2: Forward movements (steps 4-6)
        print("\n📍 Phase 2: Forward Movement Sequence")
        for i in range(3, 6):  # Last 3 movements are forward
            movement = self.MOVEMENT_SEQUENCE[i]
            print(f"\n[{i+1}/6] {movement['description']}")

            # Double-check connection before each forward command
            if not await self.verify_connection():
                print(f"❌ Connection lost before forward step {i+1}")
                return False

            success = await self.send_command(
                movement["command"],
                f"Coordinates: X={movement['x']}, Y={movement['y']}",
                0.2  # Optimal timing confirmed by user testing
            )

            if not success:
                print(f"❌ Forward movement failed at step {i+1}")
                print("🔄 This might be a timing/connection issue")
                return False



        # Transition pause and connection verification


        print("\n✅ Complete movement sequence finished!")
        return True

    def create_custom_movement(self, x: int, y: int) -> bytes:
        """Create custom movement command with specified coordinates

        WARNING: Custom commands may not work reliably!
        The robot appears to expect commands in exact sequences.
        The working commands contain context/state information beyond just coordinates.
        """
        # Base pattern: aa020900080d000000000000214545010c0000000c
        base = bytes.fromhex("aa020900080d000000000000214545010c0000000c")

        # Convert coordinates to 16-bit signed little-endian
        x_bytes = x.to_bytes(2, 'little', signed=True)
        y_bytes = y.to_bytes(2, 'little', signed=True)

        # Add one padding byte and simple checksum
        padding = bytes([0x00])
        checksum = bytes([(sum(x_bytes) + sum(y_bytes)) & 0xFF])

        return base + x_bytes + y_bytes + padding + checksum

    async def test_custom_movements(self):
        """Test custom movement commands"""
        print(f"\n🧪 Custom Movement Testing")
        print("=" * 30)

        custom_movements = [
            (0, 0, "Center/Stop"),
            (-50, 0, "Slow Backward"),
            (0, 0, "Stop"),
            (50, 0, "Slow Forward"),
            (0, 0, "Stop"),
            (-20, 30, "Backward + Right"),
            (0, 0, "Stop"),
            (20, -30, "Forward + Left"),
            (0, 0, "Final Stop")
        ]

        for x, y, desc in custom_movements:
            cmd = self.create_custom_movement(x, y)
            success = await self.send_command(cmd, f"{desc} (X={x}, Y={y})", 0.3)

            if success:
                await asyncio.sleep(1.5)  # Longer pause to observe movement
            else:
                print("❌ Custom movement failed, stopping test")
                break

    async def disconnect(self):
        """Disconnect from device"""
        if self.client and self.client.is_connected:
            try:
                # Send final stop command
                stop_cmd = self.create_custom_movement(0, 0)
                await self.send_command(stop_cmd, "Final stop before disconnect", 0.3)

                await self.client.stop_notify(self.NOTIFY_CHAR_UUID)
                await self.client.disconnect()
                print("🔗 Disconnected")
            except:
                pass

async def main():
    replicator = EcoFlowBladeMovementReplicator()

    try:
        # Connect to device
        if not await replicator.find_and_connect():
            return

        # Initialize device
        if not await replicator.initialize_device():
            print("❌ Initialization failed")
            return

        # Execute the exact working movement sequence
        if not await replicator.execute_movement_sequence():
            print("❌ Movement sequence failed")
            return

        # Test custom movements
        print("\n" + "="*50)
        user_input = input("Execute custom movement tests? (y/N): ").lower().strip()
        if user_input == 'y':
            await replicator.test_custom_movements()

    except KeyboardInterrupt:
        print("\n⏹️  Interrupted by user")
    except Exception as e:
        print(f"\n❌ Error: {e}")
    finally:
        await replicator.disconnect()

if __name__ == "__main__":
    print("🤖 EcoFlow BLADE Movement Replicator")
    print("=" * 40)
    print("Replicating the exact working movement sequence")
    print("from btsnoop analysis with optimal timing.")
    print("\n⚠️  Make sure your BLADE has clear space to move!")
    print("📋 Sequence: Backward → Stop → Backward+Turn → Forward movements\n")

    asyncio.run(main())