import re
import sys

def parse_and_verify(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Extract Bitmap Array
    bitmap_match = re.search(r'static const uint8_t glyph_bitmap\[\] = \{(.*?)\};', content, re.DOTALL)
    if not bitmap_match:
        print("Could not find glyph_bitmap")
        return

    bitmap_str = bitmap_match.group(1)
    # cleaning
    bitmap_values = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', bitmap_str)]

    # Extract DSC
    # {.bitmap_index = 0, .adv_w = 32, .box_w = 16, .box_h = 26, .ofs_x = 8, .ofs_y = -1},
    dsc_pattern = re.compile(r'\{\.bitmap_index = (\d+), \.adv_w = \d+, \.box_w = (\d+), \.box_h = (\d+)')
    dscs = dsc_pattern.findall(content)

    print(f"Found {len(dscs)} glyphs.")

    for i, (idx_str, w_str, h_str) in enumerate(dscs):
        idx = int(idx_str)
        w = int(w_str)
        h = int(h_str)

        print(f"\nGlyph {i}: {w}x{h} (Index {idx})")

        # Stride = (w + 7) // 8 bytes
        stride = (w + 7) // 8

        for r in range(h):
            row_str = ""
            for b in range(stride):
                byte_offset = idx + r * stride + b
                if byte_offset < len(bitmap_values):
                    val = bitmap_values[byte_offset]
                    # Print bits
                    for bit in range(8):
                        if (b * 8 + bit) < w:
                            if (val >> (7 - bit)) & 1:
                                row_str += "##"
                            else:
                                row_str += ".."
                else:
                    row_str += "ERR"
            print(row_str)

if __name__ == "__main__":
    parse_and_verify("EcoflowSTM32F4/src/ui/font_mdi.c")
