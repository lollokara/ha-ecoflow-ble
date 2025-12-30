import math
from PIL import Image

# Icon Paths from mdi_icons.h
ICONS = {
    "SOLAR": "M 12 7 C 9.24 7 7 9.24 7 12 C 7 14.76 9.24 17 12 17 C 14.76 17 17 14.76 17 12 C 17 9.24 14.76 7 12 7 Z M 2 13 L 2 11 L 6 11 L 6 13 L 2 13 Z M 18 13 L 22 13 L 22 11 L 18 11 L 18 13 Z M 11 2 L 13 2 L 13 6 L 11 6 L 11 2 Z M 11 18 L 13 18 L 13 22 L 11 22 L 11 18 Z M 4.93 4.93 L 3.51 6.34 L 6.34 9.17 L 7.76 7.76 L 4.93 4.93 Z M 16.24 16.24 L 14.83 17.66 L 17.66 20.49 L 19.07 19.07 L 16.24 16.24 Z M 19.07 4.93 L 17.66 6.34 L 14.83 3.51 L 16.24 2.1 L 19.07 4.93 Z M 7.76 16.24 L 6.34 17.66 L 3.51 14.83 L 4.93 13.41 L 7.76 16.24 Z",
    "AC_PLUG": "M 16 7 L 16 3 L 14 3 L 14 7 L 10 7 L 10 3 L 8 3 L 8 7 C 5.79 7 4 8.79 4 11 L 4 14 L 10 20 L 14 20 L 20 14 L 20 11 C 20 8.79 18.21 7 16 7 Z",
    "CAR_BATTERY": "M 4 6 C 2.9 6 2 6.9 2 8 L 2 17 C 2 18.1 2.9 19 4 19 L 20 19 C 21.1 19 22 18.1 22 17 L 22 8 C 22 6.9 21.1 6 20 6 L 17 6 L 17 4 L 15 4 L 15 6 L 9 6 L 9 4 L 7 4 L 7 6 L 4 6 Z M 4 8 L 20 8 L 20 17 L 4 17 L 4 8 Z M 6 11 L 8 11 L 8 13 L 10 13 L 10 11 L 12 11 L 12 14 L 6 14 L 6 11 Z M 14 11 L 18 11 L 18 13 L 14 13 L 14 11 Z",
    "BATTERY": "M 16 20 L 8 20 L 8 6 L 16 6 L 16 20 Z M 16 4 L 14 4 L 14 2 L 10 2 L 10 4 L 8 4 C 6.9 4 6 4.9 6 6 L 6 20 C 6 21.1 6.9 22 8 22 L 16 22 C 17.1 22 18 21.1 18 20 L 18 6 C 18 4.9 17.1 4 16 4 Z",
    "USB": "M 7 2 L 7 6 L 4 6 L 4 10 L 7 10 L 7 14 L 4 14 L 4 18 L 7 18 L 7 22 L 17 22 L 17 18 L 20 18 L 20 14 L 17 14 L 17 10 L 20 10 L 20 6 L 17 6 L 17 2 L 7 2 Z M 9 4 L 15 4 L 15 8 L 9 8 L 9 4 Z M 9 16 L 15 16 L 15 20 L 9 20 L 9 16 Z",
    "AC_SOCKET": "M 12 2 C 6.48 2 2 6.48 2 12 C 2 17.52 6.48 22 12 22 C 17.52 22 22 17.52 22 12 C 22 6.48 17.52 2 12 2 Z M 8 11 C 9.1 11 10 11.9 10 13 C 10 14.1 9.1 15 8 15 C 6.9 15 6 14.1 6 13 C 6 11.9 6.9 11 8 11 Z M 16 11 C 17.1 11 18 11.9 18 13 C 18 14.1 17.1 15 16 15 C 14.9 15 14 14.1 14 13 C 14 11.9 14.9 11 16 11 Z",
    "SETTINGS": "M 12 15.5 C 10.07 15.5 8.5 13.93 8.5 12 C 8.5 10.07 10.07 8.5 12 8.5 C 13.93 8.5 15.5 10.07 15.5 12 C 15.5 13.93 13.93 15.5 12 15.5 Z M 19.43 12.97 C 19.47 12.65 19.5 12.33 19.5 12 C 19.5 11.67 19.47 11.35 19.43 11.03 L 21.54 9.37 C 21.73 9.22 21.78 8.95 21.66 8.73 L 19.66 5.27 C 19.54 5.05 19.27 4.96 19.05 5.05 L 16.56 6.05 C 16.04 5.66 15.5 5.32 14.87 5.07 L 14.5 2.42 C 14.46 2.18 14.25 2 14 2 L 10 2 C 9.75 2 9.54 2.18 9.5 2.42 L 9.13 5.07 C 8.5 5.32 7.96 5.66 7.44 6.05 L 4.95 5.05 C 4.73 4.96 4.46 5.05 4.34 5.27 L 2.34 8.73 C 2.21 8.95 2.27 9.22 2.46 9.37 L 4.57 11.03 C 4.53 11.35 4.5 11.67 4.5 12 C 4.5 12.33 4.53 12.65 4.57 12.97 L 2.46 14.63 C 2.27 14.78 2.21 15.05 2.34 15.27 L 4.34 18.73 C 4.46 18.95 4.73 19.04 4.95 18.95 L 7.44 17.95 C 7.96 18.34 8.5 18.68 9.13 18.93 L 9.5 21.58 C 9.54 21.82 9.75 22 10 22 L 14 22 C 14.25 22 14.46 21.82 14.5 21.58 L 14.87 18.93 C 15.5 18.68 16.04 18.34 16.56 17.95 L 19.05 18.95 C 19.27 19.04 19.54 18.95 19.66 18.73 L 21.66 15.27 C 21.78 15.05 21.73 14.78 21.54 14.63 L 19.43 12.97 Z"
}

ICON_SIZE = 48
SUPER_SAMPLE = 4
SIZE_SS = ICON_SIZE * SUPER_SAMPLE

# Helper to parse path commands
def parse_path(path_str, scale):
    cmds = path_str.replace(',', ' ').split()
    polygons = []
    current_poly = []

    idx = 0
    cx, cy = 0, 0
    start_x, start_y = 0, 0

    while idx < len(cmds):
        c = cmds[idx]
        idx += 1

        if c == 'M':
            cx = float(cmds[idx]) * scale
            cy = float(cmds[idx+1]) * scale
            idx += 2
            start_x, start_y = cx, cy
            current_poly = [(cx, cy)]

        elif c == 'L':
            cx = float(cmds[idx]) * scale
            cy = float(cmds[idx+1]) * scale
            idx += 2
            current_poly.append((cx, cy))

        elif c == 'Z':
            current_poly.append((start_x, start_y))
            cx, cy = start_x, start_y
            polygons.append(current_poly)
            current_poly = []

        elif c == 'C':
            x1 = float(cmds[idx]) * scale; y1 = float(cmds[idx+1]) * scale
            x2 = float(cmds[idx+2]) * scale; y2 = float(cmds[idx+3]) * scale
            x  = float(cmds[idx+4]) * scale; y  = float(cmds[idx+5]) * scale
            idx += 6
            p0 = (cx, cy)
            steps = 20
            for i in range(1, steps + 1):
                t = i / float(steps)
                u = 1 - t
                tt = t*t; uu = u*u; uuu = uu*u; ttt = tt*t
                px = uuu * p0[0] + 3 * uu * t * x1 + 3 * u * tt * x2 + ttt * x
                py = uuu * p0[1] + 3 * uu * t * y1 + 3 * u * tt * y2 + ttt * y
                current_poly.append((px, py))
            cx, cy = x, y

    return polygons

def render_icon(name, path):
    img = Image.new('L', (SIZE_SS, SIZE_SS), 0)
    mdi_scale = SIZE_SS / 24.0
    polys = parse_path(path, mdi_scale)
    pixels = img.load()
    for y in range(SIZE_SS):
        intersections = []
        for poly in polys:
            if len(poly) < 2: continue
            for i in range(len(poly)):
                p1 = poly[i]
                p2 = poly[(i + 1) % len(poly)]
                if (p1[1] > y) != (p2[1] > y):
                    x = (p2[0] - p1[0]) * (y - p1[1]) / (p2[1] - p1[1]) + p1[0]
                    intersections.append(x)
        intersections.sort()
        for i in range(0, len(intersections), 2):
            if i+1 < len(intersections):
                x_start = int(intersections[i])
                x_end = int(intersections[i+1])
                for x in range(x_start, x_end):
                    if 0 <= x < SIZE_SS:
                        pixels[x, y] = 255
    # Downsample to 48x48
    img_sm = img.resize((ICON_SIZE, ICON_SIZE), resample=Image.Resampling.LANCZOS)
    return img_sm

def generate_lvgl_images():
    c_content = '#include "lvgl.h"\n\n'

    for name, path in ICONS.items():
        print(f"Rendering {name}...")
        img = render_icon(name, path)
        data = list(img.getdata())

        # Output LVGL Image Descriptor (A8 format)
        var_name = f"img_mdi_{name.lower()}"
        map_name = f"{var_name}_map"

        c_content += f"const uint8_t {map_name}[] = {{\n"
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_str = ", ".join([f"0x{b:02X}" for b in chunk])
            c_content += f"    {hex_str},\n"
        c_content += "};\n\n"

        c_content += f"const lv_image_dsc_t {var_name} = {{\n"
        c_content += "  .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
        c_content += "  .header.cf = LV_COLOR_FORMAT_A8,\n" # Use A8 for alpha-only icon
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
