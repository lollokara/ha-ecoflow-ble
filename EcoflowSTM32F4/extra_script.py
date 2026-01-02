import shutil
Import("env")

env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16"
    ]
)

def extra_bin_copy(source, target, env):
    print("Copying firmware to project directory...")
    shutil.copy(str(target[0]), env.get('PROJECT_DIR'))
    print("Done.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", extra_bin_copy)
