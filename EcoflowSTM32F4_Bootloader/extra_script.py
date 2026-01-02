Import("env")

print("Current LINKFLAGS:", env.get("LINKFLAGS"))
env.Append(LINKFLAGS=["-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"])
print("New LINKFLAGS:", env.get("LINKFLAGS"))
