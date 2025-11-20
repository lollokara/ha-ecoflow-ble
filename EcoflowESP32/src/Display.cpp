#include "Display.h"
#include "Font.h"

#define DATAPIN    8
#define CLOCKPIN   18
#define NUM_ROWS   9
#define NUM_COLS   20
#define NUMPIXELS  (NUM_ROWS * NUM_COLS)

// Adafruit_DotStar(pixels, data, clock, type)
Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

// --- Global State ---
EcoflowData currentData;

enum class MenuState {
    MAIN_MENU,
    SELECTION,
    DETAIL
};

enum class SelectionPage {
    AC,
    DC,
    USB,
    SOL
};

MenuState currentState = MenuState::MAIN_MENU;
SelectionPage currentSelection = SelectionPage::AC;

unsigned long lastInteractionTime = 0;
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
int maxScrollOffset = 0;
bool needScroll = false;

// For slide animation
bool isAnimating = false;
int animationStep = 0;
SelectionPage prevSelection = SelectionPage::AC;
int slideDirection = 0; // 1 = Up, -1 = Down

// --- Colors ---
const uint32_t COLOR_RED    = 0xFF0000; // G R B order? DotStar is usually BGR or GBR.
// Adafruit DotStar constructor is DOTSTAR_BGR.
// Color() takes (R, G, B).
// Let's use strip.Color(R, G, B)
uint32_t cRed;
uint32_t cYellow;
uint32_t cGreen;
uint32_t cWhite;
uint32_t cOff;

// --- Frame Buffer ---
// We draw to this buffer, then push to strip.
// 9 rows, 20 cols.
uint32_t frameBuffer[9][20];

// Forward Declarations
void drawMainMenu();
void drawSelectionMenu();
void drawDetailMenu();
void drawText(int x, int y, String text, uint32_t color);
void renderFrame();
void clearFrame();

// --- Helper: Map (x,y) to strip index ---
uint16_t getPixelIndex(int x, int y) {
    if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS) return NUMPIXELS;
    uint16_t base = x * NUM_ROWS;
    if (x % 2 == 0) {
        // Even columns: Top->Bottom (0->8)
        return base + y;
    } else {
        // Odd columns: Bottom->Top (8->0)
        return base + (8 - y);
    }
}

void setPixel(int x, int y, uint32_t color) {
    uint16_t index = getPixelIndex(x, y);
    if (index < NUMPIXELS) {
        strip.setPixelColor(index, color);
    }
}

void setupDisplay() {
    strip.begin();
    strip.setBrightness(25);
    strip.fill(0);
    strip.show();

    cRed = strip.Color(255, 0, 0);
    cYellow = strip.Color(255, 255, 0);
    cGreen = strip.Color(0, 255, 0);
    cWhite = strip.Color(255, 255, 255);
    cOff = strip.Color(0, 0, 0);

    lastInteractionTime = millis();
}

// --- Drawing Primitives ---

void clearFrame() {
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            frameBuffer[y][x] = cOff;
        }
    }
}

// Draw a character at (x,y). Returns width of char + spacing.
// Font is 5x7.
int drawChar(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 95) c = '?';
    const uint8_t* bitmap = font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        int dx = x + col;
        if (dx >= 0 && dx < NUM_COLS) {
            for (int row = 0; row < 7; row++) {
                int dy = y + row; // Font top is row 0
                if (dy >= 0 && dy < NUM_ROWS) {
                    if (bitmap[col] & (1 << row)) {
                        frameBuffer[dy][dx] = color;
                    }
                }
            }
        }
    }
    return 6; // 5px char + 1px space
}

void drawText(int x, int y, String text, uint32_t color) {
    int curX = x;
    for (int i = 0; i < text.length(); i++) {
        curX += drawChar(curX, y, text[i], color);
    }
}

// Calculate text width in pixels
int getTextWidth(String text) {
    return text.length() * 6; // 5px + 1px spacing
}

// --- State Machine & Logic ---

void updateDisplay(const EcoflowData& data) {
    currentData = data;

    // Check Timeouts
    unsigned long now = millis();
    if (currentState == MenuState::DETAIL) {
        if (now - lastInteractionTime > 60000) { // 60s
            currentState = MenuState::MAIN_MENU;
        }
    } else if (currentState == MenuState::SELECTION) {
        if (now - lastInteractionTime > 10000) { // 10s
            currentState = MenuState::MAIN_MENU;
        }
    }

    clearFrame();

    if (currentState == MenuState::MAIN_MENU) {
        drawMainMenu();
    } else if (currentState == MenuState::SELECTION) {
        drawSelectionMenu();
    } else if (currentState == MenuState::DETAIL) {
        drawDetailMenu();
    }

    renderFrame();
}

// --- Drawing Screens ---

void drawMainMenu() {
    int battery = currentData.batteryLevel;
    uint32_t color = cGreen;
    if (battery < 20) color = cRed;
    else if (battery < 60) color = cYellow;

    // Cap at 99 if needed to avoid scroll, per user request
    // If it's 100, let's just show 99 to keep it static as requested:
    // "no scrolling is better you can even cap it to 99 if needed"
    int displayVal = battery;
    if (displayVal > 99) displayVal = 99;

    String text = String(displayVal) + "%";
    // 5x7 font. Height 7.
    // Display height 9. Center vertically: 1px margin top/bottom. Y=1.

    // Calculate X to center it
    int width = getTextWidth(text) - 1; // Remove last spacing
    int x = (NUM_COLS - width) / 2;

    drawText(x, 1, text, color);
}

void drawSelectionMenu() {
    // Selection: AC, DC, USB, SOL
    // We need animation.

    if (isAnimating) {
        // Animation logic
        // We are sliding between prevSelection and currentSelection
        // Direction: 1 = Up (Next Page comes from bottom), -1 = Down (Next Page comes from top)
        // Wait, logic:
        // Press Down (Next): Current text goes Up, New text comes from Bottom.
        // Press Up (Prev): Current text goes Down, New text comes from Top.

        int step = animationStep; // 0 to 9 (Height)

        // Draw OLD selection
        String oldText = "";
        switch(prevSelection) {
            case SelectionPage::AC: oldText = "AC"; break;
            case SelectionPage::DC: oldText = "DC"; break;
            case SelectionPage::USB: oldText = "USB"; break;
            case SelectionPage::SOL: oldText = "SOL"; break;
        }
        int oldWidth = getTextWidth(oldText) - 1;
        int oldX = (NUM_COLS - oldWidth) / 2;

        int oldY = 1;
        if (slideDirection == 1) { // Next (Slide Up)
            oldY = 1 - step;
        } else { // Prev (Slide Down)
            oldY = 1 + step;
        }
        drawText(oldX, oldY, oldText, cWhite);

        // Draw NEW selection
        String newText = "";
        switch(currentSelection) {
            case SelectionPage::AC: newText = "AC"; break;
            case SelectionPage::DC: newText = "DC"; break;
            case SelectionPage::USB: newText = "USB"; break;
            case SelectionPage::SOL: newText = "SOL"; break;
        }
        int newWidth = getTextWidth(newText) - 1;
        int newX = (NUM_COLS - newWidth) / 2;
        
        int newY = 1;
        if (slideDirection == 1) { // Next (New comes from Bottom)
            newY = 1 + (9 - step);
        } else { // Prev (New comes from Top)
            newY = 1 - (9 - step);
        }
        drawText(newX, newY, newText, cWhite);

        animationStep+=2; // Speed
        if (animationStep > 9) {
            isAnimating = false;
        }
    } else {
        // Static draw
        String text = "";
        switch(currentSelection) {
            case SelectionPage::AC: text = "AC"; break;
            case SelectionPage::DC: text = "DC"; break;
            case SelectionPage::USB: text = "USB"; break;
            case SelectionPage::SOL: text = "SOL"; break;
        }
        int width = getTextWidth(text) - 1;
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, cWhite);
    }
}

void drawDetailMenu() {
    // Shows Value or OFF
    String text = "";
    uint32_t color = cWhite;

    switch(currentSelection) {
        case SelectionPage::AC:
            if (currentData.acOn) {
                text = String(currentData.acOutputPower) + "W";
                color = cGreen;
            } else {
                text = "OFF";
                color = cRed;
            }
            break;
        case SelectionPage::DC:
            if (currentData.dcOn) {
                text = String(currentData.dcOutputPower) + "W";
                color = cGreen;
            } else {
                text = "OFF";
                color = cRed;
            }
            break;
        case SelectionPage::USB:
            if (currentData.usbOn) {
                // USB doesn't usually report power accurately or at all in some models,
                // but let's assume it does or just show status
                // "ON" or "OFF"? User said "current power output should be shown".
                // EcoflowData has outputPower, but that's total.
                // Usually USB power isn't broken out separately in PD335, but let's see.
                // The user request says "if on the current power output should be shown".
                // If we don't have specific USB power, we might just show "ON".
                // But let's assume 0W if on. Or "ON".
                // "OFF" is 17px. "ON" is 11px.
                // Let's stick to "OFF" if off.
                if (currentData.usbOn) {
                   // For now, just show "ON" if we don't have specific USB power,
                   // or maybe 0W? The protocol usually provides DC output which combines 12V and USB often.
                   // Let's try to show "ON" for USB to be safe, or maybe DC output if it's the only thing.
                   // Actually, `dcOutputPower` is `pow_get_12v`. USB is separate?
                   // Usually they are grouped. Let's just show "ON" for USB for now unless we have data.
                   text = "ON";
                   color = cGreen;
                } else {
                    text = "OFF";
                    color = cRed;
                }
            }
            break;
        case SelectionPage::SOL:
            // Solar Input Power
            text = String(currentData.solarInputPower) + "W";
            color = cYellow; // Solar is input
            break;
    }

    int width = getTextWidth(text) - 1;

    // Scrolling Logic
    if (width > NUM_COLS) {
        // Need Scroll
        if (millis() - lastScrollTime > 150) { // Scroll speed
            scrollOffset++;
            if (scrollOffset > (width - NUM_COLS + 5)) { // +5 for pause at end
                scrollOffset = -5; // Pause at start
            }
            lastScrollTime = millis();
        }
        
        int effectiveOffset = scrollOffset;
        if (effectiveOffset < 0) effectiveOffset = 0;
        if (effectiveOffset > (width - NUM_COLS)) effectiveOffset = (width - NUM_COLS);

        drawText(-effectiveOffset, 1, text, color);

    } else {
        // Center
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, color);
        scrollOffset = 0;
    }
}

// --- Render to Strip ---
void renderFrame() {
    strip.clear();
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            uint32_t c = frameBuffer[y][x];
            if (c != 0) {
                setPixel(x, y, c);
            }
        }
    }
    strip.show();
}

// --- Input Handling ---

DisplayAction handleDisplayInput(ButtonInput input) {
    lastInteractionTime = millis();

    // Main Menu -> Any Button -> Selection Menu
    if (currentState == MenuState::MAIN_MENU) {
        currentState = MenuState::SELECTION;
        currentSelection = SelectionPage::AC; // Default to AC? Or remember last?
        // Let's Default to AC to be consistent.
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::SELECTION) {
        if (isAnimating) return DisplayAction::NONE; // Ignore while animating

        switch(input) {
            case ButtonInput::BTN_UP: // Previous Page
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::AC) currentSelection = SelectionPage::SOL;
                else currentSelection = (SelectionPage)((int)currentSelection - 1);

                // Animation: Slide Down (Text moves Down, New comes from Top)
                slideDirection = -1;
                isAnimating = true;
                animationStep = 0;
                break;

            case ButtonInput::BTN_DOWN: // Next Page
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::SOL) currentSelection = SelectionPage::AC;
                else currentSelection = (SelectionPage)((int)currentSelection + 1);

                // Animation: Slide Up (Text moves Up, New comes from Bottom)
                slideDirection = 1;
                isAnimating = true;
                animationStep = 0;
                break;

            case ButtonInput::BTN_ENTER_SHORT:
                currentState = MenuState::DETAIL;
                scrollOffset = -5; // Reset scroll
                break;

            default:
                break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::DETAIL) {
        switch(input) {
            case ButtonInput::BTN_ENTER_LONG:
                // Toggle Action!
                switch(currentSelection) {
                    case SelectionPage::AC: return DisplayAction::TOGGLE_AC;
                    case SelectionPage::DC: return DisplayAction::TOGGLE_DC;
                    case SelectionPage::USB: return DisplayAction::TOGGLE_USB;
                    default: return DisplayAction::NONE;
                }
                break;

            case ButtonInput::BTN_UP:
            case ButtonInput::BTN_DOWN:
            case ButtonInput::BTN_ENTER_SHORT:
                // Reset timeout is handled by lastInteractionTime update
                break;
        }
        return DisplayAction::NONE;
    }

    return DisplayAction::NONE;
}
