import os

Import("env")

def merge_firmware(source, target, env):
    print("Merging firmware...")

    # Paths
    bootloader_path = os.path.join(env.subst("$PROJECT_DIR"), "..", "EcoflowSTM32F4_Bootloader", ".pio", "build", "disco_f469ni", "firmware.bin")
    app_path = source[0].get_abspath()
    output_path = os.path.join(env.subst("$PROJECT_DIR"), "factory_firmware.bin")

    if not os.path.exists(bootloader_path):
        print(f"Warning: Bootloader not found at {bootloader_path}. Skipping merge.")
        return

    # Read Binaries
    with open(bootloader_path, "rb") as f:
        bootloader_data = f.read()

    with open(app_path, "rb") as f:
        app_data = f.read()

    # Padding
    # Sector 0: Bootloader (16KB)
    # Sector 1: Config (16KB) - Fill with 0xFF
    # Total Offset for App: 32KB (0x8000)

    bootloader_size = len(bootloader_data)
    if bootloader_size > 0x4000:
        print(f"Error: Bootloader too large ({bootloader_size} > 16KB)")
        return

    padding1 = b'\xFF' * (0x4000 - bootloader_size)
    config_padding = b'\xFF' * 0x4000

    # Combine
    full_image = bootloader_data + padding1 + config_padding + app_data

    with open(output_path, "wb") as f:
        f.write(full_image)

    print(f"Created factory_firmware.bin at {output_path}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
