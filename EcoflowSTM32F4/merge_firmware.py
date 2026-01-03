import os

Import("env")

def merge_firmware(source, target, env):
    # Paths
    bootloader_path = "../EcoflowSTM32F4_Bootloader/.pio/build/disco_f469ni/firmware.bin"
    # source[0] is often the ELF file when attached to the BIN target.
    # target[0] is the BIN file.
    app_path = str(target[0])
    output_path = os.path.join(env.subst("$PROJECT_DIR"), "factory_firmware.bin")

    # Constants
    BOOTLOADER_SIZE = 0x4000 # 16KB
    CONFIG_SIZE = 0x4000     # 16KB
    APP_OFFSET = 0x8000      # 32KB

    print("Merging firmware... ")

    if not os.path.exists(bootloader_path):
        print(f"Warning: Bootloader not found at {bootloader_path}. Skipping merge.")
        return

    try:
        with open(bootloader_path, "rb") as f:
            boot_bin = f.read()

        with open(app_path, "rb") as f:
            app_bin = f.read()

        # Pad Bootloader to 16KB
        if len(boot_bin) > BOOTLOADER_SIZE:
            print("Error: Bootloader too large!")
            return

        padding1 = b'\xFF' * (BOOTLOADER_SIZE - len(boot_bin))

        # Empty Config (16KB)
        config_padding = b'\xFF' * CONFIG_SIZE

        # Merge
        with open(output_path, "wb") as f:
            f.write(boot_bin)
            f.write(padding1)
            f.write(config_padding)
            f.write(app_bin)

        print(f"Factory firmware created at: {output_path}")

    except Exception as e:
        print(f"Error merging firmware: {e}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
