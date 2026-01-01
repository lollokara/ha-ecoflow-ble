Import("env")
import shutil
import os

# Force FPU flags into linker
env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16"
    ]
)

def copy_bin(source, target, env):
    bin_path = str(target[0])
    project_dir = env.subst("$PROJECT_DIR")
    target_path = os.path.join(project_dir, "firmware.bin")
    shutil.copy(bin_path, target_path)
    print(f"Firmware copied to {target_path}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bin)
