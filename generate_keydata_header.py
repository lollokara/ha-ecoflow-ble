import sys
import os

# Add the python implementation to the path to import keydata
sys.path.append(os.path.abspath('Python Implementation/custom_components/ef_ble/eflib'))

from keydata import _data

def generate_header(data_bytes, output_path):
    with open(output_path, "w") as f:
        f.write("#ifndef CREDENTIALS_H\n")
        f.write("#define CREDENTIALS_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("static const char* ECOFLOW_USER_ID = \"\";\n")
        f.write("static const char* ECOFLOW_DEVICE_SN = \"\";\n\n")
        f.write("const uint8_t ECOFLOW_KEYDATA[] = {\n    ")
        
        for i, byte in enumerate(data_bytes):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 16 == 0:
                f.write("\n    ")
        
        f.write("\n}\n\n")
        f.write("#endif // CREDENTIALS_H\n")

if __name__ == "__main__":
    output_file = "EcoflowESP32/src/Credentials.h"
    generate_header(_data, output_file)
    print(f"Generated {output_file}")
