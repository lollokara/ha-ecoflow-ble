import os

Import("env")

def merge_firmware(source, target, env):
    # Paths
    bootloader_path = "../BrainTransplantCore/.pio/build/disco_f469ni/firmware.bin"

    # Ensure we use the .bin file, not the .elf file
    # SCons might pass the .elf as source[0], so we explicitly construct the bin path
    app_path = os.path.join(env.subst("$BUILD_DIR"), env.subst("${PROGNAME}.bin"))

    output_path = os.path.join(env.subst("$PROJECT_DIR"), "factory_firmware.bin")

    # Constants
    BOOTLOADER_SIZE = 0x4000 # 16KB
    CONFIG_SIZE = 0x4000     # 16KB
    APP_OFFSET = 0x8000      # 32KB

    print(f"Merging firmware... ")
    print(f"App Path: {app_path}")

    if not os.path.exists(bootloader_path):
        print(f"Warning: Bootloader not found at {bootloader_path}. Skipping merge.")
        return

    if not os.path.exists(app_path):
        print(f"Error: Application binary not found at {app_path}")
        return

    try:
        with open(bootloader_path, "rb") as f:
            boot_bin = f.read()

        with open(app_path, "rb") as f:
            app_bin = f.read()

        # Check Bootloader Size
        boot_len = len(boot_bin)
        print(f"Bootloader Size: {boot_len} bytes")
        if boot_len > BOOTLOADER_SIZE:
            raise Exception(f"Error: Bootloader too large! Size: {boot_len} bytes, Max: {BOOTLOADER_SIZE} bytes.")

        padding1 = b'\xFF' * (BOOTLOADER_SIZE - boot_len)

        # Empty Config (16KB)
        config_padding = b'\xFF' * CONFIG_SIZE

        # Verify App Binary isn't ELF (Check Magic)
        if len(app_bin) > 4 and app_bin[0:4] == b'\x7FELF':
             raise Exception(f"Error: Application binary appears to be an ELF file! Check your build settings.")

        # Merge
        with open(output_path, "wb") as f:
            f.write(boot_bin)
            f.write(padding1)
            f.write(config_padding)
            f.write(app_bin)

        print(f"Factory firmware created at: {output_path}")

    except Exception as e:
        print(f"Error merging firmware: {e}")
        env.Exit(1)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)
