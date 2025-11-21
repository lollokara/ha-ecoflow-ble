#include "Display.h"
#include "Font.h"
#include <math.h>
#include <vector>

#define DATAPIN    8
#define CLOCKPIN   18
#define NUM_ROWS   9
#define NUM_COLS   20
#define NUMPIXELS  (NUM_ROWS * NUM_COLS)

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

EcoflowData currentData;

// --- State Machine ---
enum class MenuState {
    MAIN_MENU,
    SELECTION, // AC, DC, USB, SOL, IN, SET
    SETTINGS_SUBMENU, // CHG, LIM
    LIMITS_SUBMENU, // UP, DN
    DETAIL // Edit or View
};

// Selection Items (Global pool)
enum class SelectionItem {
    // Root Selection
    PAGE_AC, PAGE_DC, PAGE_USB, PAGE_SOL, PAGE_IN, PAGE_SET,
    // Settings Submenu
    PAGE_CHG, PAGE_LIM,
    // Limits Submenu
    PAGE_UP, PAGE_DN
};

MenuState currentState = MenuState::MAIN_MENU;
SelectionItem currentSelection = SelectionItem::PAGE_AC;

// Previous selection storage for submenus to return to correct spot?
// For now, we just use defaults or currentSelection works if unique.
// Since SelectionItems are unique, `currentSelection` tells us where we are.

unsigned long lastInteractionTime = 0;
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
bool needScroll = false;

// Animations
bool isAnimating = false;
int animationStep = 0;
SelectionItem prevSelection = SelectionItem::PAGE_AC;
int slideDirection = 0;

DisplayAction pendingAction = DisplayAction::NONE;

// Colors
uint32_t cRed, cYellow, cGreen, cWhite, cOff, cBlue;
uint32_t frameBuffer[9][20];

// Settings Temp Values
int tempAcLimit = 400;
int tempMaxCharge = 100;
int tempMinDischarge = 0;

// Prototypes
void drawMainMenu();
void drawSelectionMenu(String items[], int count, int selectedIndex);
void drawDetailMenu();
void drawNcScreen();
void drawText(int x, int y, String text, uint32_t color);
void renderFrame();
void clearFrame();
int getTextWidth(String text);
void navigateSelection(int direction); // 1=Next, -1=Prev

uint16_t getPixelIndex(int x, int y) {
    if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS) return NUMPIXELS;
    uint16_t base = x * NUM_ROWS;
    if (x % 2 == 0) return base + y;
    else return base + (8 - y);
}

void setPixel(int x, int y, uint32_t color) {
    uint16_t index = getPixelIndex(x, y);
    if (index < NUMPIXELS) strip.setPixelColor(index, color);
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

void clearFrame() {
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            frameBuffer[y][x] = cOff;
        }
    }
}

int drawChar(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 95) c = '?';
    const uint8_t* bitmap = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        int dx = x + col;
        if (dx >= 0 && dx < NUM_COLS) {
            for (int row = 0; row < 7; row++) {
                int dy = y + row;
                if (dy >= 0 && dy < NUM_ROWS) {
                    if (bitmap[col] & (1 << row)) frameBuffer[dy][dx] = color;
                }
            }
        }
    }
    return 6;
}

void drawText(int x, int y, String text, uint32_t color) {
    int curX = x;
    for (int i = 0; i < text.length(); i++) {
        if (curX >= NUM_COLS) break;
        if (curX + 6 >= 0) drawChar(curX, y, text[i], color);
        curX += 6;
    }
}

int getTextWidth(String text) {
    return text.length() * 6;
}

// --- Logic ---

void updateDisplay(const EcoflowData& data) {
    currentData = data;
    clearFrame();

    if (!currentData.isConnected) {
        strip.setBrightness(25);
        drawNcScreen();
        renderFrame();
        return;
    }

    // Timeouts
    unsigned long now = millis();
    unsigned long timeout = 10000; // Default 10s
    if (currentState == MenuState::DETAIL) timeout = 60000;
    else if (currentState == MenuState::MAIN_MENU) timeout = 0; // Infinite

    if (timeout > 0 && (now - lastInteractionTime > timeout)) {
        // Handle SET/Edit timeouts confirming values
        if (currentState == MenuState::DETAIL) {
            if (currentSelection == SelectionItem::PAGE_CHG || currentSelection == SelectionItem::PAGE_SET) { // SET legacy mapped to CHG?
                pendingAction = DisplayAction::SET_AC_LIMIT;
            } else if (currentSelection == SelectionItem::PAGE_UP) {
                pendingAction = DisplayAction::SET_MAX_CHG;
            } else if (currentSelection == SelectionItem::PAGE_DN) {
                pendingAction = DisplayAction::SET_MIN_DSG;
            }
        }
        currentState = MenuState::MAIN_MENU;
    }

    if (currentState == MenuState::MAIN_MENU) {
        drawMainMenu();
    } else if (currentState == MenuState::SELECTION) {
        strip.setBrightness(25);
        // Map currentSelection to index?
        // Items: AC, DC, USB, SOL, IN, SET
        // SelectionItem enums are contiguous.
        drawSelectionMenu(nullptr, 0, 0); // Logic handled inside
    } else if (currentState == MenuState::SETTINGS_SUBMENU) {
        strip.setBrightness(25);
        drawSelectionMenu(nullptr, 0, 0);
    } else if (currentState == MenuState::LIMITS_SUBMENU) {
        strip.setBrightness(25);
        drawSelectionMenu(nullptr, 0, 0);
    } else if (currentState == MenuState::DETAIL) {
        strip.setBrightness(25);
        drawDetailMenu();
    }

    renderFrame();
}

void drawNcScreen() {
    String text = "NC";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cRed);
}

void drawMainMenu() {
    int battery = currentData.batteryLevel;
    uint32_t color = cGreen;
    bool isCharging = (currentData.inputPower > 0);

    if (isCharging) {
        color = cGreen;
        float t = (float)millis() / 2000.0f;
        float val = (sin(t) + 1.0f) / 2.0f;
        int minB = 7; int maxB = 51;
        strip.setBrightness(minB + (int)(val * (maxB - minB)));
    } else {
        strip.setBrightness(25);
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

String getItemText(SelectionItem item) {
    switch(item) {
        case SelectionItem::PAGE_AC: return "AC";
        case SelectionItem::PAGE_DC: return "DC";
        case SelectionItem::PAGE_USB: return "USB";
        case SelectionItem::PAGE_SOL: return "SOL";
        case SelectionItem::PAGE_IN: return "IN";
        case SelectionItem::PAGE_SET: return "SET";
        case SelectionItem::PAGE_CHG: return "CHG";
        case SelectionItem::PAGE_LIM: return "LIM";
        case SelectionItem::PAGE_UP: return "UP";
        case SelectionItem::PAGE_DN: return "DN";
        default: return "?";
    }
}

void drawSelectionMenu(String* dummy, int c, int idx) {
    // We use global currentSelection
    if (isAnimating) {
        int step = animationStep;
        String oldText = getItemText(prevSelection);
        String newText = getItemText(currentSelection);

        int oldWidth = getTextWidth(oldText) - 1;
        int oldX = (NUM_COLS - oldWidth) / 2;
        int oldY = (slideDirection == 1) ? (1 - step) : (1 + step);
        drawText(oldX, oldY, oldText, cWhite);

        int newWidth = getTextWidth(newText) - 1;
        int newX = (NUM_COLS - newWidth) / 2;
        int newY = (slideDirection == 1) ? (1 + (9 - step)) : (1 - (9 - step));
        drawText(newX, newY, newText, cWhite);

        animationStep+=2;
        if (animationStep > 9) isAnimating = false;
    } else {
        String text = getItemText(currentSelection);
        int width = getTextWidth(text) - 1;
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, cWhite);
    }
}

void drawDetailMenu() {
    String text = "";
    uint32_t color = cWhite;

    if (currentSelection == SelectionItem::PAGE_SET || currentSelection == SelectionItem::PAGE_CHG) {
        // Show AC Charging Limit
        // If entering SET, we are in detail? No, SET is a submenu now.
        // But if we are in DETAIL, and selection is PAGE_CHG (or SET legacy).
        int displayNum = tempAcLimit / 10;
        text = String(displayNum);
        color = cBlue;
    } else if (currentSelection == SelectionItem::PAGE_UP) {
        text = String(tempMaxCharge) + "%";
        color = cBlue;
    } else if (currentSelection == SelectionItem::PAGE_DN) {
        text = String(tempMinDischarge) + "%";
        color = cBlue;
    } else {
        // View Pages
        switch(currentSelection) {
            case SelectionItem::PAGE_AC:
                if (currentData.acOn) {
                    text = String(currentData.acOutputPower) + "W";
                    color = cGreen;
                } else { text = "OFF"; color = cRed; }
                break;
            case SelectionItem::PAGE_DC:
                if (currentData.dcOn) {
                    text = String(currentData.dcOutputPower) + "W";
                    color = cGreen;
                } else { text = "OFF"; color = cRed; }
                break;
            case SelectionItem::PAGE_USB:
                if (currentData.usbOn) { text = "ON"; color = cGreen; }
                else { text = "OFF"; color = cRed; }
                break;
            case SelectionItem::PAGE_SOL:
                text = String(currentData.solarInputPower) + "W";
                color = cYellow;
                break;
            case SelectionItem::PAGE_IN:
                text = String(currentData.inputPower) + "W";
                color = cYellow;
                break;
            default: break;
        }
    }

    int width = getTextWidth(text) - 1;
    if (width > NUM_COLS) {
        if (millis() - lastScrollTime > 150) {
            scrollOffset++;
            if (scrollOffset > (width - NUM_COLS + 5)) scrollOffset = -5;
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
            if (c != 0) setPixel(x, y, c);
        }
    }
    strip.show();
}

// --- Navigation Helpers ---

void navigateSelection(int direction) {
    // Determine list based on state
    std::vector<SelectionItem> items;
    if (currentState == MenuState::SELECTION) {
        items = {SelectionItem::PAGE_AC, SelectionItem::PAGE_DC, SelectionItem::PAGE_USB, SelectionItem::PAGE_SOL, SelectionItem::PAGE_IN, SelectionItem::PAGE_SET};
    } else if (currentState == MenuState::SETTINGS_SUBMENU) {
        items = {SelectionItem::PAGE_CHG, SelectionItem::PAGE_LIM};
    } else if (currentState == MenuState::LIMITS_SUBMENU) {
        items = {SelectionItem::PAGE_UP, SelectionItem::PAGE_DN};
    } else {
        return;
    }

    int idx = -1;
    for (int i=0; i<items.size(); i++) {
        if (items[i] == currentSelection) { idx = i; break; }
    }
    if (idx == -1) idx = 0; // Safety

    prevSelection = currentSelection;
    int newIdx = idx + direction;
    if (newIdx < 0) newIdx = items.size() - 1; // Wrap
    if (newIdx >= items.size()) newIdx = 0; // Wrap

    currentSelection = items[newIdx];

    slideDirection = direction;
    isAnimating = true;
    animationStep = 0;
}

// --- Input Handling ---

DisplayAction handleDisplayInput(ButtonInput input) {
    lastInteractionTime = millis();

    // Global Back (Very Long Press)
    if (input == ButtonInput::BTN_ENTER_VERY_LONG) {
        // Go up one level
        if (currentState == MenuState::DETAIL) {
            // If in edit, exit without saving? Or save?
            // Usually "Back" cancels.
            // Determine parent menu
            if (currentSelection == SelectionItem::PAGE_CHG || currentSelection == SelectionItem::PAGE_LIM) {
                currentState = MenuState::SETTINGS_SUBMENU;
            } else if (currentSelection == SelectionItem::PAGE_UP || currentSelection == SelectionItem::PAGE_DN) {
                currentState = MenuState::LIMITS_SUBMENU;
            } else {
                currentState = MenuState::SELECTION; // View pages
            }
        } else if (currentState == MenuState::LIMITS_SUBMENU) {
            currentState = MenuState::SETTINGS_SUBMENU;
            currentSelection = SelectionItem::PAGE_LIM;
        } else if (currentState == MenuState::SETTINGS_SUBMENU) {
            currentState = MenuState::SELECTION;
            currentSelection = SelectionItem::PAGE_SET;
        } else if (currentState == MenuState::SELECTION) {
            currentState = MenuState::MAIN_MENU;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::MAIN_MENU) {
        if (input != ButtonInput::BTN_ENTER_VERY_LONG) {
            currentState = MenuState::SELECTION;
            currentSelection = SelectionItem::PAGE_AC;
            strip.setBrightness(25);
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::SELECTION || currentState == MenuState::SETTINGS_SUBMENU || currentState == MenuState::LIMITS_SUBMENU) {
        if (isAnimating) return DisplayAction::NONE;

        switch(input) {
            case ButtonInput::BTN_UP: navigateSelection(-1); break;
            case ButtonInput::BTN_DOWN: navigateSelection(1); break;
            case ButtonInput::BTN_ENTER_SHORT:
                // Enter Logic
                if (currentSelection == SelectionItem::PAGE_SET) {
                    currentState = MenuState::SETTINGS_SUBMENU;
                    currentSelection = SelectionItem::PAGE_CHG;
                } else if (currentSelection == SelectionItem::PAGE_LIM) {
                    currentState = MenuState::LIMITS_SUBMENU;
                    currentSelection = SelectionItem::PAGE_UP;
                } else if (currentSelection == SelectionItem::PAGE_CHG) {
                    currentState = MenuState::DETAIL;
                    // Sync temp value?
                    // We don't have current set limit in EcoflowData, only what we last set?
                    // Or we assume 400? Better to persist. `tempAcLimit` persists.
                } else if (currentSelection == SelectionItem::PAGE_UP) {
                    currentState = MenuState::DETAIL;
                    tempMaxCharge = currentData.maxChargeLevel;
                } else if (currentSelection == SelectionItem::PAGE_DN) {
                    currentState = MenuState::DETAIL;
                    tempMinDischarge = currentData.minDischargeLevel;
                } else {
                    // View Page
                    currentState = MenuState::DETAIL;
                }
                scrollOffset = -5;
                break;
            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::DETAIL) {
        // Edit Logic
        if (currentSelection == SelectionItem::PAGE_CHG) {
             switch(input) {
                case ButtonInput::BTN_UP:
                    tempAcLimit += 100;
                    if (tempAcLimit > 1500) tempAcLimit = 1500;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempAcLimit -= 100;
                    if (tempAcLimit < 400) tempAcLimit = 400;
                    break;
                case ButtonInput::BTN_ENTER_LONG: // Confirm
                    currentState = MenuState::SETTINGS_SUBMENU;
                    return DisplayAction::SET_AC_LIMIT;
                default: break;
             }
        } else if (currentSelection == SelectionItem::PAGE_UP) { // Max Charge
             switch(input) {
                case ButtonInput::BTN_UP:
                    tempMaxCharge += 5; // 5% increments?
                    if (tempMaxCharge > 100) tempMaxCharge = 100;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempMaxCharge -= 5;
                    if (tempMaxCharge < 50) tempMaxCharge = 50;
                    break;
                case ButtonInput::BTN_ENTER_LONG:
                    currentState = MenuState::LIMITS_SUBMENU;
                    return DisplayAction::SET_MAX_CHG;
                default: break;
             }
        } else if (currentSelection == SelectionItem::PAGE_DN) { // Min Discharge
             switch(input) {
                case ButtonInput::BTN_UP:
                    tempMinDischarge += 5;
                    if (tempMinDischarge > 30) tempMinDischarge = 30;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempMinDischarge -= 5;
                    if (tempMinDischarge < 0) tempMinDischarge = 0;
                    break;
                case ButtonInput::BTN_ENTER_LONG:
                    currentState = MenuState::LIMITS_SUBMENU;
                    return DisplayAction::SET_MIN_DSG;
                default: break;
             }
        } else {
            // Toggles for View Pages
            if (input == ButtonInput::BTN_ENTER_LONG) {
                switch(currentSelection) {
                    case SelectionItem::PAGE_AC: return DisplayAction::TOGGLE_AC;
                    case SelectionItem::PAGE_DC: return DisplayAction::TOGGLE_DC;
                    case SelectionItem::PAGE_USB: return DisplayAction::TOGGLE_USB;
                    default: return DisplayAction::NONE;
                }
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

int getSetAcLimit() { return tempAcLimit; }
int getSetMaxCharge() { return tempMaxCharge; }
int getSetMinDischarge() { return tempMinDischarge; }
