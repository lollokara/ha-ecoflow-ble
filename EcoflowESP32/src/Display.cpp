#include "Display.h"
#include "Font.h"
#include <math.h>

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
    SOL,
    IN,
    SET
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

// Pending Action for Timeout
DisplayAction pendingAction = DisplayAction::NONE;

// --- Colors ---
uint32_t cRed;
uint32_t cYellow;
uint32_t cGreen;
uint32_t cWhite;
uint32_t cOff;
uint32_t cBlue;

// --- Frame Buffer ---
// 9 rows, 20 cols.
uint32_t frameBuffer[9][20];

// --- Settings State ---
int tempAcLimit = 400;

// Forward Declarations
void drawMainMenu();
void drawSelectionMenu();
void drawDetailMenu();
void drawNcScreen();
void drawText(int x, int y, String text, uint32_t color);
void renderFrame();
void clearFrame();
int getTextWidth(String text);

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
    cBlue = strip.Color(0, 0, 255);

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
        if (curX >= NUM_COLS) break;
        if (curX + 6 >= 0) {
            drawChar(curX, y, text[i], color);
        }
        curX += 6;
    }
}

// Calculate text width in pixels
int getTextWidth(String text) {
    return text.length() * 6; // 5px + 1px spacing
}

// --- State Machine & Logic ---

void updateDisplay(const EcoflowData& data) {
    currentData = data;
    clearFrame();

    // Priority 1: Disconnected
    if (!currentData.isConnected) {
        strip.setBrightness(25);
        drawNcScreen();
        renderFrame();
        return;
    }

    // Check Timeouts
    unsigned long now = millis();
    if (currentState == MenuState::DETAIL) {
        if (currentSelection == SelectionPage::SET) {
             if (now - lastInteractionTime > 60000) { // 60s
                // Timeout confirms SET
                pendingAction = DisplayAction::SET_AC_LIMIT;
                currentState = MenuState::MAIN_MENU;
            }
        } else {
            if (now - lastInteractionTime > 60000) { // 60s
                currentState = MenuState::MAIN_MENU;
            }
        }
    } else if (currentState == MenuState::SELECTION) {
        if (now - lastInteractionTime > 10000) { // 10s
            currentState = MenuState::MAIN_MENU;
        }
    }

    if (currentState == MenuState::MAIN_MENU) {
        drawMainMenu();
    } else if (currentState == MenuState::SELECTION) {
        // Reset brightness in case we came from breathing
        strip.setBrightness(25);
        drawSelectionMenu();
    } else if (currentState == MenuState::DETAIL) {
        strip.setBrightness(25);
        drawDetailMenu();
    }

    renderFrame();
}

// --- Drawing Screens ---

void drawNcScreen() {
    String text = "NC";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cRed);
}

void drawMainMenu() {
    int battery = currentData.batteryLevel;
    uint32_t color = cGreen;

    // Check Charging
    bool isCharging = (currentData.inputPower > 0);

    if (isCharging) {
        color = cGreen;

        // Breathing Animation: 3% (7) to 20% (51)
        // Period approx 4s?
        // sin^2 gives smooth 0..1
        float t = (float)millis() / 2000.0f; // Slow down
        float val = (sin(t) + 1.0f) / 2.0f; // 0.0 to 1.0

        int minB = 7;
        int maxB = 51;
        int brightness = minB + (int)(val * (maxB - minB));
        strip.setBrightness(brightness);

    } else {
        strip.setBrightness(25); // Default static
        if (battery < 20) color = cRed;
        else if (battery < 60) color = cYellow;
        else color = cGreen;
    }

    int displayVal = battery;
    if (displayVal > 99) displayVal = 99;

    String text = String(displayVal) + "%";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;

    drawText(x, 1, text, color);
}

String getSelectionText(SelectionPage page) {
    switch(page) {
        case SelectionPage::AC: return "AC";
        case SelectionPage::DC: return "DC";
        case SelectionPage::USB: return "USB";
        case SelectionPage::SOL: return "SOL";
        case SelectionPage::IN: return "IN";
        case SelectionPage::SET: return "SET";
        default: return "?";
    }
}

void drawSelectionMenu() {
    if (isAnimating) {
        int step = animationStep;

        // Draw OLD selection
        String oldText = getSelectionText(prevSelection);
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
        String newText = getSelectionText(currentSelection);
        int newWidth = getTextWidth(newText) - 1;
        int newX = (NUM_COLS - newWidth) / 2;

        int newY = 1;
        if (slideDirection == 1) { // Next (New comes from Bottom)
            newY = 1 + (9 - step);
        } else { // Prev (New comes from Top)
            newY = 1 - (9 - step);
        }
        drawText(newX, newY, newText, cWhite);

        animationStep+=2;
        if (animationStep > 9) {
            isAnimating = false;
        }
    } else {
        String text = getSelectionText(currentSelection);
        int width = getTextWidth(text) - 1;
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, cWhite);
    }
}

void drawDetailMenu() {
    String text = "";
    uint32_t color = cWhite;

    if (currentSelection == SelectionPage::SET) {
        int displayNum = tempAcLimit / 10;
        text = String(displayNum);
        color = cBlue;
    } else {
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
                   text = "ON";
                   color = cGreen;
                } else {
                    text = "OFF";
                    color = cRed;
                }
                break;
            case SelectionPage::SOL:
                text = String(currentData.solarInputPower) + "W";
                color = cYellow;
                break;
            case SelectionPage::IN:
                text = String(currentData.inputPower) + "W";
                color = cYellow;
                break;
            default: break;
        }
    }

    int width = getTextWidth(text) - 1;

    // Scrolling Logic
    if (width > NUM_COLS) {
        if (millis() - lastScrollTime > 150) {
            scrollOffset++;
            if (scrollOffset > (width - NUM_COLS + 5)) {
                scrollOffset = -5;
            }
            lastScrollTime = millis();
        }
        
        int effectiveOffset = scrollOffset;
        if (effectiveOffset < 0) effectiveOffset = 0;
        if (effectiveOffset > (width - NUM_COLS)) effectiveOffset = (width - NUM_COLS);

        drawText(-effectiveOffset, 1, text, color);

    } else {
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, color);
        scrollOffset = 0;
    }
}

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

    // If not connected, input shouldn't do anything or just wake screen?
    // Requirement doesn't specify. We'll assume normal handling,
    // but Main loop prevents actions if not authenticated anyway.
    // But display transitions might still happen.
    // If "NC" is shown, we probably shouldn't allow menu navigation?
    // Let's leave it as is; `updateDisplay` will override drawing with NC anyway.

    if (currentState == MenuState::MAIN_MENU) {
        currentState = MenuState::SELECTION;
        currentSelection = SelectionPage::AC;
        strip.setBrightness(25);
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::SELECTION) {
        if (isAnimating) return DisplayAction::NONE;

        switch(input) {
            case ButtonInput::BTN_UP: // Previous Page
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::AC) currentSelection = SelectionPage::SET;
                else currentSelection = (SelectionPage)((int)currentSelection - 1);

                slideDirection = -1;
                isAnimating = true;
                animationStep = 0;
                break;

            case ButtonInput::BTN_DOWN: // Next Page
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::SET) currentSelection = SelectionPage::AC;
                else currentSelection = (SelectionPage)((int)currentSelection + 1);

                slideDirection = 1;
                isAnimating = true;
                animationStep = 0;
                break;

            case ButtonInput::BTN_ENTER_SHORT:
                currentState = MenuState::DETAIL;
                scrollOffset = -5;
                break;

            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::DETAIL) {
        if (currentSelection == SelectionPage::SET) {
             // SET Menu Logic
             switch(input) {
                case ButtonInput::BTN_UP:
                    tempAcLimit += 100;
                    if (tempAcLimit > 1500) tempAcLimit = 1500;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempAcLimit -= 100;
                    if (tempAcLimit < 400) tempAcLimit = 400;
                    break;
                case ButtonInput::BTN_ENTER_LONG:
                    currentState = MenuState::MAIN_MENU;
                    return DisplayAction::SET_AC_LIMIT;

                case ButtonInput::BTN_ENTER_SHORT:
                    break;
             }
             return DisplayAction::NONE;

        } else {
            // Normal Toggle Logic
            switch(input) {
                case ButtonInput::BTN_ENTER_LONG:
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
                    break;
            }
        }
        return DisplayAction::NONE;
    }

    return DisplayAction::NONE;
}

DisplayAction getPendingAction() {
    DisplayAction act = pendingAction;
    pendingAction = DisplayAction::NONE;
    return act;
}

int getSetAcLimit() {
    return tempAcLimit;
}
