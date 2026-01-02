Import("env")
import shutil
import os

def copy_firmware(source, target, env):
    firmware_path = str(target[0])
    target_path = os.path.join(env["PROJECT_DIR"], "firmware.bin")

    print(f"Copying {firmware_path} to {target_path}")
    shutil.copy(firmware_path, target_path)

env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16"
    ]
)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)
