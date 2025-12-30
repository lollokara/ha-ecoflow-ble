#!/usr/bin/env python3
import sys
import subprocess
import os

# Helper script to generate MDI font
# Usage: ./generate_mdi_font.py <output_file.c>

def generate_font():
    # Requirements: nodejs, lv_font_conv
    # MDI font file path (assumed to be in fonts/)
    font_path = "EcoflowSTM32F4/fonts/materialdesignicons-webfont.ttf"

    if not os.path.exists(font_path):
        print(f"Error: Font file not found at {font_path}")
        # Try to find it
        found = False
        for root, dirs, files in os.walk("."):
            if "materialdesignicons-webfont.ttf" in files:
                font_path = os.path.join(root, "materialdesignicons-webfont.ttf")
                found = True
                break
        if not found:
            print("Could not find materialdesignicons-webfont.ttf. Please ensure it exists.")
            return

    # Icons to include (Hex codes from ui_icons.h and Memory)
    # Solar: F0A72
    # Plug: F06A5
    # Car: F010C
    # USB: F1CBF
    # Settings: F0493
    # Back: F004D
    # Battery: F0079
    # AC: F1107
    # Fan: F0210
    # Thermometer: F050F
    # Fire: F0237 (Memory) / F023E (Old) - Memory says F0237
    # Snowflake: F04AE (Memory) / F0494 (Old) - Memory says F04AE
    # Leaf: F030D
    # Moon: F0586 (Memory) / F0594 (Old) - Memory says F0586
    # Power: F0425
    # Sun: F0599 (Memory)

    icons = [
        "0xF0A72", "0xF06A5", "0xF010C", "0xF1CBF",
        "0xF0493", "0xF004D", "0xF0079", "0xF1107",
        "0xF0210", "0xF050F", "0xF0237", "0xF04AE",
        "0xF030D", "0xF0586", "0xF0425", "0xF0599"
    ]

    range_arg = ",".join(icons)
    output_file = "EcoflowSTM32F4/src/ui/ui_font_mdi.c"

    cmd = [
        "lv_font_conv",
        "--font", font_path,
        "--range", range_arg,
        "--size", "28",
        "--bpp", "4",
        "--format", "lvgl",
        "--no-compress",
        "--output", output_file
    ]

    print(f"Running: {' '.join(cmd)}")

    try:
        subprocess.run(cmd, check=True)
        print(f"Successfully generated {output_file}")
    except FileNotFoundError:
        print("Error: lv_font_conv not found. Please install it via 'npm install -g lv_font_conv'")
    except subprocess.CalledProcessError as e:
        print(f"Error generating font: {e}")

if __name__ == "__main__":
    generate_font()
