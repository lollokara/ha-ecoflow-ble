from PIL import Image, ImageFont, ImageDraw
import sys

# Mapping from icon name (our key) to MDI Codepoint (Hex)
# I found these by searching the CSS content provided.
ICON_MAP = {
    "SOLAR": 0xF0A72, # mdi-solar-power
    "AC_PLUG": 0xF06A5, # mdi-power-plug
    "CAR_BATTERY": 0xF010C, # mdi-car-battery
    "BATTERY": 0xF0079, # mdi-battery
    "USB": 0xF11F0, # mdi-usb-port
    "AC_SOCKET": 0xF07E7, # mdi-power-socket-eu (or similar)
    "SETTINGS": 0xF0493 # mdi-cog
}

FONT_FILE = "materialdesignicons-webfont.ttf"
ICON_SIZE = 48

def generate_lvgl_images():
    try:
        font = ImageFont.truetype(FONT_FILE, ICON_SIZE)
    except IOError:
        print("Error: TTF file not found.")
        sys.exit(1)

    c_content = '#include "lvgl.h"\n\n'

    for name, codepoint in ICON_MAP.items():
        print(f"Rendering {name} from TTF...")

        # Create a temporary image to draw the glyph
        # Size needs to be big enough
        img = Image.new('L', (ICON_SIZE, ICON_SIZE), 0)
        draw = ImageDraw.Draw(img)

        # Draw the glyph centered.
        # ImageDraw.text coordinates are usually top-left.
        # We need to center it. Get bounding box.
        text = chr(codepoint)
        bbox = font.getbbox(text)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]

        # Centering logic: (Canvas - Text) / 2 - BBoxOffset
        x = (ICON_SIZE - text_w) / 2 - bbox[0]
        y = (ICON_SIZE - text_h) / 2 - bbox[1]

        draw.text((x, y), text, font=font, fill=255)

        # Output LVGL Image Descriptor (A8 format)
        var_name = f"img_mdi_{name.lower()}"
        map_name = f"{var_name}_map"
        data = list(img.getdata())

        c_content += f"const uint8_t {map_name}[] = {{\n"
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_str = ", ".join([f"0x{b:02X}" for b in chunk])
            c_content += f"    {hex_str},\n"
        c_content += "};\n\n"

        c_content += f"const lv_image_dsc_t {var_name} = {{\n"
        c_content += "  .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
        c_content += "  .header.cf = LV_COLOR_FORMAT_A8,\n"
        c_content += "  .header.flags = 0,\n"
        c_content += f"  .header.w = {ICON_SIZE},\n"
        c_content += f"  .header.h = {ICON_SIZE},\n"
        c_content += f"  .header.stride = {ICON_SIZE},\n"
        c_content += f"  .data_size = sizeof({map_name}),\n"
        c_content += f"  .data = {map_name},\n"
        c_content += "};\n\n"

    with open("EcoflowSTM32F4/src/img_mdi.c", "w") as f:
        f.write(c_content)

if __name__ == "__main__":
    generate_lvgl_images()
