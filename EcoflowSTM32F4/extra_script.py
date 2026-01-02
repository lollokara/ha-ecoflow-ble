Import("env")
import shutil

env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16"
    ]
)

def copy_firmware(source, target, env):
    shutil.copyfile(str(target[0]), "firmware.bin")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
