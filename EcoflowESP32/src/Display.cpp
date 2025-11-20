#include "Display.h"

#define DATAPIN    8
#define CLOCKPIN   18
#define NUM_ROWS   9
#define NUM_COLS   20
#define NUMPIXELS  (NUM_ROWS * NUM_COLS)

// Initialize DotStar strip
// Use software SPI for specific pins if hardware SPI doesn't map easily, or hardware SPI if pins match.
// Adafruit_DotStar(pixels, data, clock, type)
Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

void setupDisplay() {
    strip.begin();
    strip.setBrightness(25); // 10% of 255
    strip.fill(strip.Color(0,0,0));
    strip.show();
}

// Map logical x (0-19), y (0-8) to physical index (Vertical ZigZag)
// Row 0 is Top, Row 8 is Bottom.
// Column 0 (x=0) goes Top->Bottom (Index 0 at top, 8 at bottom).
// Column 1 (x=1) goes Bottom->Top (Index 9 at bottom, 17 at top).
uint16_t getPixelIndex(int x, int y) {
    if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS) return NUMPIXELS; // Out of bounds

    uint16_t base = x * NUM_ROWS;
    if (x % 2 == 0) {
        // Even columns: Top->Bottom (0->8)
        // Index = base + y
        return base + y;
    } else {
        // Odd columns: Bottom->Top (8->0)
        // Index = base + (8 - y)
        return base + (8 - y);
    }
}

void setPixel(int x, int y, uint32_t color) {
    uint16_t index = getPixelIndex(x, y);
    if (index < NUMPIXELS) {
        strip.setPixelColor(index, color);
    }
}

// x: 0 to 19 (Horizontal position)
// y: 0 to 8 (Vertical row)
// Logical layout:
// y=0: Row 1 (Top)
// ...
// y=8: Row 9 (Bottom)

void drawBar(int y, float value, float maxVal) {
    // Cap value
    if (value < 0) value = 0;
    if (value > maxVal) value = maxVal;

    // Calculate number of lit pixels (0 to 20)
    int litPixels = 0;
    if (maxVal > 0) {
        litPixels = (int)((value / maxVal) * NUM_COLS);
    }
    if (litPixels > NUM_COLS) litPixels = NUM_COLS;

    for (int x = 0; x < NUM_COLS; x++) {
        uint32_t color = 0; // Off
        
        if (x < litPixels) {
            if (x < 4) {
                color = strip.Color(255, 0, 0); // Red
            } else if (x < 13) {
                color = strip.Color(255, 255, 0); // Yellow
            } else {
                color = strip.Color(0, 255, 0); // Green
            }
        }
        
        setPixel(x, y, color);
    }
}

void updateDisplay(const EcoflowData& data) {
    strip.clear();

    // Row 1 (y=0): Battery % (0-100)
    drawBar(0, (float)data.batteryLevel, 100.0f);

    // Row 3 (y=2): Input Power (0-1500W)
    drawBar(2, (float)data.inputPower, 1500.0f);

    // Row 5 (y=4): AC Output (0-3000W)
    drawBar(4, (float)data.acOutputPower, 3000.0f);

    // Row 7 (y=6): DC Output (0-100W)
    drawBar(6, (float)data.dcOutputPower, 100.0f);

    // Row 9 (y=8): Solar Input (0-600W)
    drawBar(8, (float)data.solarInputPower, 600.0f);

    strip.show();
}
