import math
from PIL import Image, ImageDraw

# Icon Paths from mdi_icons.h (Manual copy to ensure matching)
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
CANVAS_SIZE = ICON_SIZE * SUPER_SAMPLE

# Simple Path Parser to Draw commands
def draw_svg_path(draw, path, scale):
    commands = path.replace(',', ' ').split()
    cmd_idx = 0

    cx, cy = 0, 0
    start_x, start_y = 0, 0

    # We will build a list of points/ops for drawing
    # PIL ImageDraw is limited, we might need to approximate curves or use polygon
    # But for "implement the font", high quality is key.
    # We can use simple line subdivision for curves.

    while cmd_idx < len(commands):
        cmd = commands[cmd_idx]
        cmd_idx += 1

        if cmd == 'M':
            x = float(commands[cmd_idx]) * scale
            y = float(commands[cmd_idx+1]) * scale
            cmd_idx += 2
            cx, cy = x, y
            start_x, start_y = cx, cy

        elif cmd == 'L':
            x = float(commands[cmd_idx]) * scale
            y = float(commands[cmd_idx+1]) * scale
            cmd_idx += 2
            draw.line([(cx, cy), (x, y)], fill=255, width=int(scale)) # Fill logic is tricky.
            # Wait, SVG is usually filled shapes. PIL.draw.polygon is needed.
            # But parsing SVG to polygon is hard.
            # We are writing a script.
            # Let's try to just use `draw.polygon` if we can collect points.
            # But paths have holes (compound paths).
            cx, cy = x, y

        elif cmd == 'Z':
            # draw.line([(cx, cy), (start_x, start_y)], fill=255, width=int(scale))
            cx, cy = start_x, start_y

        # ... Implementing a full SVG rasterizer in raw Python/PIL is hard without `svgpath` or similar.
        # But wait, I only have ~7 icons. I can just render them as filled polygons?
        # The paths are complex (holes in Settings, etc).
        # Actually, let's use the simplest approach:
        # Since I can't easily rasterize filled complex SVG in pure PIL without extensive code,
        # AND I don't have `cairo` or `svglib`...

        # Maybe I can cheat?
        # The "wonky" look came from Aliasing.
        # If I output my Vector Code to C, I have the same problem unless I implement AA there.
        # If I generate Bitmaps here, I need to render them here.

        # I will implement a minimal "scanline rasterizer" or just use a simplified version.
        # OR: I can just define the "font" data for these icons manually? No.

        # Alternative: The "wonky" might be because of line drawing.
        # What if I map the icons to simple geometric shapes that PIL CAN draw?
        pass

# Actually, writing a full SVG rasterizer script is risky.
# Let's look at the `mdi_icons.h` again.
# The `SOLAR` icon is circles and lines.
# The `SETTINGS` is complex.

# BETTER PLAN:
# I will use the `ui_vector.c` I wrote, but I will ADD Anti-Aliasing (Wu's algorithm) to `BSP_LCD_DrawLine`.
# That fixes "wonky" (aliasing) in the embedded code directly, without needing complex offline tools.
# AND I will create a `font_mdi.c` that effectively wraps these vector calls?
# No, "implement the font" implies bitmap fonts usually.
# But generating them is the blocker.

# Wait, I installed `pillow`. `pillow` doesn't handle SVG.
# But I can implement the same logic as my C code (Line, Bezier) in Python to draw into a high-res image, then downscale.
# In Python, I can just plot points.
# If I super-sample (10x), plotting points along the path (filled or stroked) is easy.
# For filled shapes, I can use a winding rule rasterizer? Too complex.
# Most MDI icons are "filled".
# My C code drew "lines" (stroked).
# Material Design icons are FILLED shapes.
# THAT IS WHY IT LOOKS WONKY! I was drawing outlines of filled shapes!
# "M 12 7 C..." is a shape boundary. If I just draw the boundary, it looks thin and weird.
# I need to FILL the shape.
# `BSP_LCD_FillPolygon` exists!
# If I convert the Bezier curves to a series of points (flattening), I can pass them to `BSP_LCD_FillPolygon`.
# This is a much better interpretation of "implement the font correctly".

# Plan Update:
# 1. Update `ui_vector.c` to support `UI_DrawSVGPathFilled`.
# 2. This function will flatten the path into a `Point` array.
# 3. Then call `BSP_LCD_FillPolygon`.
# 4. NOTE: `BSP_LCD_FillPolygon` handles convex polygons well. Concave/Complex (with holes)?
#    Standard FillPolygon often uses scanline, which handles complex shapes (odd-even rule).
#    Let's check `stm32469i_discovery_lcd.c`.
#    `BSP_LCD_FillPolygon` -> calls `FillTriangle` repeatedly. It creates a fan from the first point.
#    This ONLY works for Convex polygons. MDI icons are complex/concave.
#    So `BSP_LCD_FillPolygon` will FAIL on "Settings" or "Solar".

# So I DO need a bitmap rasterizer.
# Writing a rasterizer in Python (offline) is safer than on MCU.
# I will implement a basic "Point-in-Polygon" rasterizer in Python.
# For 48x48 icon, I can iterate every pixel (2304), check `is_inside(path)`.
# `is_inside` using ray casting is easy to implement.
# This gives me a perfect mask.
# I can then output the A8 bitmap.
# This solves everything: "Filled" (correct look), "AA" (if I supersample), "Font" (standard bitmap).

pass
