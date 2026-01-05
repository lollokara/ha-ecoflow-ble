Import("env")

env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16"
    ]
)

# This function will be called after the firmware is built
def copy_firmware(source, target, env):
    import shutil
    import os

    # Get the project directory
    project_dir = env.subst("$PROJECT_DIR")

    # Construct the path to the firmware.bin file
    firmware_path = target[0].get_path()
    firmware_filename = os.path.basename(firmware_path)

    # Define the destination directory
    bin_dir = os.path.join(project_dir, "bin")

    # Create the bin directory if it doesn't exist
    if not os.path.exists(bin_dir):
        os.makedirs(bin_dir)
        print(f"Created directory: {bin_dir}")

    # Define the destination path
    destination_path = os.path.join(bin_dir, firmware_filename)

    # Copy the file
    shutil.copy(firmware_path, destination_path)
    print(f"Copied {firmware_path} to {destination_path}")

# Add the post-build action to the firmware target
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)
