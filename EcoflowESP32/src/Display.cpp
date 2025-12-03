#include "Display.h"
#include "Font.h"
#include "LightSensor.h"
#include <math.h>

#define DATAPIN    14
#define CLOCKPIN   13
#define NUM_ROWS   9
#define NUM_COLS   20
#define NUMPIXELS  (NUM_ROWS * NUM_COLS)

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

// --- Global State ---
EcoflowData currentData;

enum class MenuState {
    DASHBOARD,
    SELECTION,
    DETAIL,
    SETTINGS_SUBMENU,
    LIMITS_SUBMENU,
    EDIT_CHG,
    EDIT_SOC_UP,
    EDIT_SOC_DN,
    DEVICE_SELECT,
    DEVICE_ACTION,
    WAVE2_MENU,
    EDIT_W2_PWR,
    EDIT_W2_MOD,
    EDIT_W2_SPD,
    EDIT_W2_SMD
};

enum class DashboardView {
    D3_BATT,
    D3_SOLAR,
    W2_BATT,
    W2_TEMP,
    D3P_BATT,
    AC_BATT, // Alternator Charger Battery %
    AC_VOLT, // Alternator Charger Input Voltage
    AC_POWER // Alternator Charger Output Power
};

enum class SelectionPage {
    AC,
    AIR,
    DC,
    USB,
    SOL,
    IN,
    SET,
    DEV
};

enum class Wave2MenuPage {
    PWR,
    MOD,
    SPD,
    SMD
};

enum class SettingsPage {
    CHG,
    LIM,
    OFF
};

enum class LimitsPage {
    UP,
    DN
};

enum class DevicePage {
    D3,
    W2,
    D3P,
    CHG
};

enum class DeviceActionPage {
    CON,
    DIS
};

MenuState currentState = MenuState::DASHBOARD;
DashboardView currentDashboardView = DashboardView::D3_BATT;
SelectionPage currentSelection = SelectionPage::AC;
SettingsPage currentSettingsPage = SettingsPage::CHG;
LimitsPage currentLimitsPage = LimitsPage::UP;
DevicePage currentDevicePage = DevicePage::D3;
DeviceActionPage currentDeviceAction = DeviceActionPage::CON;

unsigned long lastInteractionTime = 0;
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
bool needScroll = false;

// For slide animation
bool isAnimating = false;
int animationStep = 0;
SelectionPage prevSelection = SelectionPage::AC;
int slideDirection = 0; // 1 = Up, -1 = Down

// Pending Action for Timeout
DisplayAction pendingAction = DisplayAction::NONE;
DeviceType targetDeviceType = DeviceType::DELTA_3;
Wave2MenuPage currentWave2Page = Wave2MenuPage::PWR;

// Temp variables for editing
int tempAcLimit = 400;
int tempMaxChg = 100;
int tempMinDsg = 0;
int tempW2Val = 0;

// --- Colors ---
uint32_t cRed, cYellow, cGreen, cWhite, cOff, cBlue;
uint32_t frameBuffer[9][20];

// --- Helper: Map (x,y) to strip index ---
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

void renderFrame() {
    strip.clear();
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            if(frameBuffer[y][x] != 0) setPixel(x, y, frameBuffer[y][x]);
        }
    }
    strip.show();
}

// --- Brightness Control ---
float currentBrightness = 25.0f;
bool isNightMode = false;

void updateBrightness() {
    int adc = LightSensor::getInstance().getRaw();
    int minAdc = LightSensor::getInstance().getMin();
    int maxAdc = LightSensor::getInstance().getMax();

    // Normalize ADC to 0.0 - 1.0
    float norm = 0;
    if (maxAdc > minAdc) {
        norm = (float)(adc - minAdc) / (float)(maxAdc - minAdc);
    }
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;

    // Map to 1% - 50% (2.55 - 127.5)
    // strip.setBrightness takes 0-255
    float target = 2.55f + (norm * (177.5f - 2.55f));

    // Smooth transition
    if (abs(target - currentBrightness) > 0.5f) {
        currentBrightness += (target - currentBrightness) * 0.1f;
    } else {
        currentBrightness = target;
    }

    // Hysteresis for Night Mode (10% = 25.5)
    float pct = (currentBrightness / 255.0f) * 100.0f;
    if (isNightMode) {
        if (pct > 12.0f) isNightMode = false;
    } else {
        if (pct < 10.0f) isNightMode = true;
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

void drawCompactFloat(float val, uint32_t color) {
    // Format: "12.5" - tight packing for 3 digits + dot
    int intPart = (int)val;
    int decPart = (int)((val - intPart) * 10) % 10;

    String sInt = String(intPart);
    String sDec = String(decPart);

    // Calculate total width
    // Digits: 5px + 1px spacing = 6px
    // Dot: 1px + 1px spacing = 2px
    int width = (sInt.length() * 6) + 2 + 5;

    int x = (NUM_COLS - width) / 2;
    if (x < 0) x = 0;

    // Draw Int part
    for(int i=0; i<sInt.length(); i++) {
        drawChar(x, 1, sInt[i], color);
        x += 6;
    }

    // Draw Dot (Custom)
    x -= 1;
    setPixel(x, 7, color); // Dot at bottom
    x += 2; // Advance 2px (dot + space)

    // Draw Dec part
    drawChar(x, 1, sDec[0], color);
}

int getTextWidth(String text) {
    return text.length() * 6;
}

void flashScreen(uint32_t color) {
    strip.fill(color);
    strip.show();
    delay(100); // Blocking delay for feedback
    strip.fill(0);
}

DeviceType getActiveDeviceType() {
    if (DeviceManager::getInstance().getSlot(DeviceType::DELTA_PRO_3)->isConnected) return DeviceType::DELTA_PRO_3;
    if (DeviceManager::getInstance().getSlot(DeviceType::DELTA_3)->isConnected) return DeviceType::DELTA_3;
    return DeviceType::DELTA_3;
}

void drawDashboard(DeviceSlot* slotD3, DeviceSlot* slotW2, DeviceSlot* slotD3P, DeviceSlot* slotAC) {
    uint32_t color = cGreen;
    String text = "";
    int brightness = (int)currentBrightness; // Default to mediated brightness

    switch(currentDashboardView) {
        case DashboardView::D3_BATT:
            if (!slotD3->isConnected) { text = "NC"; color = cRed; }
            else {
                int batt = slotD3->instance->getBatteryLevel();

                // Charging Animation (Breathing)
                // Only if not Night Mode and charging
                if (!isNightMode && slotD3->instance->getInputPower() > 0) {
                     float t = (float)millis() / 2000.0f;
                     float val = (sin(t) + 1.0f) / 2.0f; // 0.0 to 1.0
                     // Breathe +15% brightness (approx +38 in 0-255 scale)
                     // Base is currentBrightness
                     brightness = (int)currentBrightness + (int)(val * 38.0f);
                     if (brightness > 255) brightness = 255;
                }

                if (batt > 99) batt = 99;
                text = String(batt) + "%";
            }
            break;
        case DashboardView::D3_SOLAR:
            if (!slotD3->isConnected) { text = "NC"; color = cRed; }
            else {
                text = String(slotD3->instance->getSolarInputPower());
                color = cYellow;
            }
            break;
        case DashboardView::W2_BATT:
             if (!slotW2->isConnected) { text = "NC"; color = cRed; }
             else {
                 int batt = slotW2->instance->getBatteryLevel();
                 text = String(batt > 99 ? 99 : batt) + "%";
                 color = cBlue;
             }
             break;
        case DashboardView::W2_TEMP:
            if (!slotW2->isConnected) { text = "NC"; color = cRed; }
            else {
                text = String(slotW2->instance->getAmbientTemperature()) + "C";
                color = cWhite;
            }
            break;
        case DashboardView::D3P_BATT:
            if (!slotD3P->isConnected) { text = "NC"; color = cRed; }
            else {
                 int batt = (int)currentData.deltaPro3.batteryLevel;

                 // Charging Animation for D3P
                 if (!isNightMode && currentData.deltaPro3.inputPower > 0) {
                     float t = (float)millis() / 2000.0f;
                     float val = (sin(t) + 1.0f) / 2.0f;
                     brightness = (int)currentBrightness + (int)(val * 38.0f);
                     if (brightness > 255) brightness = 255;
                 }

                 if (batt > 99) batt = 99;
                 text = String(batt) + "%";
                 color = cGreen;
            }
            break;
        case DashboardView::AC_BATT:
            if (!slotAC->isConnected) { text = "NC"; color = cRed; }
            else {
                 int batt = (int)currentData.alternatorCharger.batteryLevel;
                 text = String(batt > 99 ? 99 : batt) + "%";
                 color = cYellow;
            }
            break;
        case DashboardView::AC_VOLT:
            // Input Voltage
            {
                float volts = currentData.alternatorCharger.carBatteryVoltage;
                drawCompactFloat(volts, cBlue);
                strip.setBrightness(brightness);
                strip.show();
                return;
            }
            break;
        case DashboardView::AC_POWER:
            // DC Output Power (Signed)
            {
                int pwr = (int)currentData.alternatorCharger.dcPower;
                text = String(pwr);
                color = (pwr > 0) ? cGreen : (pwr < 0 ? cRed : cWhite);
            }
            break;
    }

    strip.setBrightness(brightness);
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);

    if (isNightMode) {
        if ((millis() / 1000) % 2 == 0) {
            setPixel(19, 8, cGreen);
        }
    }
}

String getSelectionText(SelectionPage page) {
    switch(page) {
        case SelectionPage::AC: return "AC";
        case SelectionPage::DC: return "DC";
        case SelectionPage::USB: return "USB";
        case SelectionPage::SOL: return "SOL";
        case SelectionPage::IN: return "IN";
        case SelectionPage::SET: return "SET";
        case SelectionPage::DEV: return "DEV";
        case SelectionPage::AIR: return "AIR";
        default: return "?";
    }
}

void drawSelectionMenu() {
    if (isAnimating) {
        int step = animationStep;
        String oldText = getSelectionText(prevSelection);
        int oldWidth = getTextWidth(oldText) - 1;
        int oldX = (NUM_COLS - oldWidth) / 2;
        int oldY = (slideDirection == 1) ? (1 - step) : (1 + step);
        drawText(oldX, oldY, oldText, cWhite);

        String newText = getSelectionText(currentSelection);
        int newWidth = getTextWidth(newText) - 1;
        int newX = (NUM_COLS - newWidth) / 2;
        int newY = (slideDirection == 1) ? (1 + (9 - step)) : (1 - (9 - step));
        drawText(newX, newY, newText, cWhite);

        animationStep+=2;
        if (animationStep > 9) isAnimating = false;
    } else {
        String text = getSelectionText(currentSelection);
        int width = getTextWidth(text) - 1;
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, cWhite);
    }
}

void drawDetailMenu(DeviceType activeType) {
    String text = "";
    uint32_t color = cWhite;

    bool acOn = false;
    int acOut = 0;
    bool dcOn = false;
    int dcOut = 0;
    bool usbOn = false;
    int solIn = 0;
    int totIn = 0;

    switch (activeType) {
        case DeviceType::DELTA_3:
            acOn = currentData.delta3.acOn;
            acOut = abs((int)currentData.delta3.acOutputPower);
            dcOn = currentData.delta3.dcOn;
            dcOut = abs((int)currentData.delta3.dc12vOutputPower);
            usbOn = currentData.delta3.usbOn;
            solIn = (int)currentData.delta3.solarInputPower;
            totIn = (int)currentData.delta3.inputPower;
            break;
        case DeviceType::WAVE_2:
            acOn = (currentData.wave2.mode != 0);
            acOut = 0;
            dcOn = (currentData.wave2.powerMode != 0);
            dcOut = currentData.wave2.psdrPwrWatt;
            usbOn = false;
            solIn = currentData.wave2.mpptPwrWatt;
            totIn = (currentData.wave2.batPwrWatt > 0) ? currentData.wave2.batPwrWatt : 0;
            break;
        case DeviceType::DELTA_PRO_3:
            acOn = currentData.deltaPro3.acLvPort || currentData.deltaPro3.acHvPort;
            acOut = (int)(currentData.deltaPro3.acLvOutputPower + currentData.deltaPro3.acHvOutputPower);
            dcOn = currentData.deltaPro3.dc12vPort;
            dcOut = (int)currentData.deltaPro3.dc12vOutputPower;
            usbOn = false;
            solIn = (int)(currentData.deltaPro3.solarLvPower + currentData.deltaPro3.solarHvPower);
            totIn = (int)currentData.deltaPro3.inputPower;
            break;
        case DeviceType::ALTERNATOR_CHARGER:
            acOn = false;
            acOut = 0;
            dcOn = currentData.alternatorCharger.chargerOpen;
            dcOut = (int)currentData.alternatorCharger.dcPower;
            usbOn = false;
            solIn = 0;
            totIn = 0;
            break;
        default:
            acOn = false;
            acOut = 0;
            dcOn = false;
            dcOut = 0;
            usbOn = false;
            solIn = 0;
            totIn = 0;
            break;
    }

    switch(currentSelection) {
        case SelectionPage::AC:
            text = acOn ? String(acOut) + "W" : "OFF";
            color = acOn ? cGreen : cRed;
            break;
        case SelectionPage::DC:
            text = dcOn ? String(dcOut) + "W" : "OFF";
            color = dcOn ? cGreen : cRed;
            break;
        case SelectionPage::USB:
            text = usbOn ? "ON" : "OFF";
            color = usbOn ? cGreen : cRed;
            break;
        case SelectionPage::SOL:
            text = String(solIn) + "W";
            color = cYellow;
            break;
        case SelectionPage::IN:
            text = String(totIn) + "W";
            color = cYellow;
            break;
        default: break;
    }
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawWave2Menu() {
    String text = "";
    uint32_t color = cWhite;
    bool showSmd = (currentData.wave2.mode == 0 || currentData.wave2.mode == 1);

    if (currentState == MenuState::WAVE2_MENU) {
        switch(currentWave2Page) {
            case Wave2MenuPage::PWR: text = "PWR"; break;
            case Wave2MenuPage::MOD: text = "MOD"; break;
            case Wave2MenuPage::SPD: text = "SPD"; break;
            case Wave2MenuPage::SMD:
                 if (!showSmd) { currentWave2Page = Wave2MenuPage::PWR; text="PWR"; }
                 else text = "SMD";
                 break;
        }
    } else {
        if (currentState == MenuState::EDIT_W2_PWR) {
            text = (tempW2Val == 1) ? "ON" : "OFF";
            color = (tempW2Val == 1) ? cGreen : cRed;
        } else if (currentState == MenuState::EDIT_W2_MOD) {
            switch(tempW2Val) {
                case 0: text = "ICE"; color = cBlue; break;
                case 1: text = "HOT"; color = cRed; break;
                case 2: text = "AIR"; color = cWhite; break;
            }
        } else if (currentState == MenuState::EDIT_W2_SPD) {
            text = String(tempW2Val + 1);
            color = cYellow;
        } else if (currentState == MenuState::EDIT_W2_SMD) {
            switch(tempW2Val) {
                case 0: text = "MAX"; break;
                case 1: text = "NGT"; break;
                case 2: text = "ECO"; break;
                case 3: text = "NOR"; break;
            }
        }
    }

    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawSettingsSubmenu() {
    String text = "CHG";
    if (currentSettingsPage == SettingsPage::LIM) text = "LIM";
    else if (currentSettingsPage == SettingsPage::OFF) text = "OFF";

    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cWhite);
}

void drawLimitsSubmenu() {
    String text = (currentLimitsPage == LimitsPage::UP) ? "UP" : "DN";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cWhite);
}

void drawEditScreen(String label, int value, String unit) {
    String text;
    if (currentState == MenuState::EDIT_CHG) {
        text = String(value);
    } else {
        text = String(value) + unit;
    }

    uint32_t color = cBlue;
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawDeviceSelectMenu(DeviceSlot* slotD3, DeviceSlot* slotW2, DeviceSlot* slotD3P, DeviceSlot* slotAC) {
    String text = "";
    bool connected = false;
    switch(currentDevicePage) {
        case DevicePage::D3: text = "D3"; connected = slotD3->isConnected; break;
        case DevicePage::W2: text = "W2"; connected = slotW2->isConnected; break;
        case DevicePage::D3P: text = "D3P"; connected = slotD3P->isConnected; break;
        case DevicePage::CHG: text = "CHG"; connected = slotAC->isConnected; break;
    }
    uint32_t color = connected ? cGreen : cRed;
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawDeviceActionMenu(DeviceSlot* slot) {
    String text = "";
    uint32_t color = cWhite;
    switch(currentDeviceAction) {
        case DeviceActionPage::CON: text = "CON"; color = cBlue; break;
        case DeviceActionPage::DIS: text = "DIS"; color = cRed; break;
    }
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

// --- State Machine & Logic ---

void updateDisplay(const EcoflowData& data, DeviceSlot* activeSlot, bool isScanning) {
    currentData = data;
    DeviceSlot* slotD3 = DeviceManager::getInstance().getSlot(DeviceType::DELTA_3);
    DeviceSlot* slotW2 = DeviceManager::getInstance().getSlot(DeviceType::WAVE_2);
    DeviceSlot* slotD3P = DeviceManager::getInstance().getSlot(DeviceType::DELTA_PRO_3);
    DeviceSlot* slotAC = DeviceManager::getInstance().getSlot(DeviceType::ALTERNATOR_CHARGER);

    // Auto-switch Dashboard View based on priority
    if (currentState == MenuState::DASHBOARD) {
        // If D3P is connected and D3 is NOT, and we are looking at D3 BATT, switch to D3P BATT
        if (slotD3P->isConnected && !slotD3->isConnected && currentDashboardView == DashboardView::D3_BATT) {
            currentDashboardView = DashboardView::D3P_BATT;
        }
        // If D3 is connected and D3P is NOT, and we are looking at D3P BATT, switch to D3 BATT
        if (slotD3->isConnected && !slotD3P->isConnected && currentDashboardView == DashboardView::D3P_BATT) {
            currentDashboardView = DashboardView::D3_BATT;
        }
        // Priority D3P if both connected? User said "Delta 3 Pro first"
        if (slotD3->isConnected && slotD3P->isConnected) {
             if (currentDashboardView == DashboardView::D3_BATT) currentDashboardView = DashboardView::D3P_BATT;
        }
    }

    updateBrightness();
    clearFrame();

    unsigned long now = millis();
    unsigned long timeout = 10000; // Default 10s
    if (currentState == MenuState::DASHBOARD) timeout = 0; // No timeout
    else if (currentState == MenuState::EDIT_CHG || currentState == MenuState::EDIT_SOC_UP || currentState == MenuState::EDIT_SOC_DN ||
             currentState == MenuState::EDIT_W2_PWR || currentState == MenuState::EDIT_W2_MOD || currentState == MenuState::EDIT_W2_SPD || currentState == MenuState::EDIT_W2_SMD) {
        timeout = 60000; // 60s for editing
    }

    if (timeout > 0 && (now - lastInteractionTime > timeout)) {
        currentState = MenuState::DASHBOARD;
    }

    if (currentState == MenuState::DASHBOARD) {
        drawDashboard(slotD3, slotW2, slotD3P, slotAC);
    } else if (currentState == MenuState::SELECTION) {
        drawSelectionMenu();
    } else if (currentState == MenuState::DETAIL) {
        DeviceType activeType = activeSlot ? activeSlot->type : DeviceType::DELTA_3;
        drawDetailMenu(activeType);
    } else if (currentState == MenuState::SETTINGS_SUBMENU) {
        drawSettingsSubmenu();
    } else if (currentState == MenuState::LIMITS_SUBMENU) {
        drawLimitsSubmenu();
    } else if (currentState == MenuState::EDIT_CHG) {
        drawEditScreen("CHG", tempAcLimit, "");
    } else if (currentState == MenuState::EDIT_SOC_UP) {
        drawEditScreen("UP", tempMaxChg, "%");
    } else if (currentState == MenuState::EDIT_SOC_DN) {
        drawEditScreen("DN", tempMinDsg, "%");
    } else if (currentState == MenuState::DEVICE_SELECT) {
        drawDeviceSelectMenu(slotD3, slotW2, slotD3P, slotAC);
    } else if (currentState == MenuState::DEVICE_ACTION) {
        DeviceSlot* s = nullptr;
        if (currentDevicePage == DevicePage::D3) s = slotD3;
        else if (currentDevicePage == DevicePage::W2) s = slotW2;
        else if (currentDevicePage == DevicePage::D3P) s = slotD3P;
        else if (currentDevicePage == DevicePage::CHG) s = slotAC;
        drawDeviceActionMenu(s);
    } else if (currentState == MenuState::WAVE2_MENU || currentState == MenuState::EDIT_W2_PWR || currentState == MenuState::EDIT_W2_MOD || currentState == MenuState::EDIT_W2_SPD || currentState == MenuState::EDIT_W2_SMD) {
        drawWave2Menu();
    }

    if (isScanning) setPixel(19, 0, cBlue);
    renderFrame();
}

DisplayAction handleDisplayInput(ButtonInput input) {
    lastInteractionTime = millis();

    // Global Back Logic (Hold > 1s)
    if (input == ButtonInput::BTN_ENTER_HOLD) {
        switch(currentState) {
            case MenuState::DASHBOARD:
                break; // Ignore
            case MenuState::SELECTION:
                currentState = MenuState::DASHBOARD; break;
            case MenuState::DETAIL:
                currentState = MenuState::SELECTION; break;
            case MenuState::SETTINGS_SUBMENU:
                currentState = MenuState::SELECTION; break;
            case MenuState::LIMITS_SUBMENU:
                currentState = MenuState::SETTINGS_SUBMENU; break;
            case MenuState::EDIT_CHG:
                currentState = MenuState::SETTINGS_SUBMENU; break;
            case MenuState::EDIT_SOC_UP:
            case MenuState::EDIT_SOC_DN:
                currentState = MenuState::LIMITS_SUBMENU; break;
            case MenuState::DEVICE_SELECT:
                currentState = MenuState::SELECTION; break;
            case MenuState::DEVICE_ACTION:
                currentState = MenuState::DEVICE_SELECT; break;
            case MenuState::WAVE2_MENU:
                currentState = MenuState::SELECTION; break;
            case MenuState::EDIT_W2_PWR:
            case MenuState::EDIT_W2_MOD:
            case MenuState::EDIT_W2_SPD:
            case MenuState::EDIT_W2_SMD:
                currentState = MenuState::WAVE2_MENU; break;
            default:
                currentState = MenuState::DASHBOARD; break;
        }
        return DisplayAction::NONE;
    }

    switch(currentState) {
        case MenuState::DASHBOARD:
            if (input == ButtonInput::BTN_UP || input == ButtonInput::BTN_DOWN) {
                 int currentIdx = (int)currentDashboardView;
                 int dir = (input == ButtonInput::BTN_UP) ? 1 : -1;

                 // Loop to find next valid view
                 int nextIdx = currentIdx;
                 for(int i=0; i<8; i++) { // Max 8 views
                     nextIdx += dir;
                     if (nextIdx > 7) nextIdx = 0;
                     if (nextIdx < 0) nextIdx = 7;

                     // Check validity
                     DeviceSlot* slotAC = DeviceManager::getInstance().getSlot(DeviceType::ALTERNATOR_CHARGER);
                     bool acConnected = slotAC && slotAC->isConnected;

                     if (nextIdx == (int)DashboardView::AC_VOLT || nextIdx == (int)DashboardView::AC_POWER) {
                         if (acConnected) break; // Valid
                         // Else continue loop
                     } else {
                         break; // Others always valid (show NC)
                     }
                 }
                 currentDashboardView = (DashboardView)nextIdx;
            } else if (input == ButtonInput::BTN_ENTER_SHORT) {
                currentState = MenuState::SELECTION;
                currentSelection = SelectionPage::AC;
            }
            break;

        case MenuState::SELECTION:
            switch(input) {
                case ButtonInput::BTN_UP:
                    prevSelection = currentSelection;
                    if (currentSelection == SelectionPage::AC) currentSelection = SelectionPage::DEV; // Wrap last
                    else currentSelection = (SelectionPage)((int)currentSelection - 1);
                    slideDirection = -1; isAnimating = true; animationStep = 0;
                    break;
                case ButtonInput::BTN_DOWN:
                    prevSelection = currentSelection;
                    if (currentSelection == SelectionPage::DEV) currentSelection = SelectionPage::AC; // Wrap first
                    else currentSelection = (SelectionPage)((int)currentSelection + 1);
                    slideDirection = 1; isAnimating = true; animationStep = 0;
                    break;
                case ButtonInput::BTN_ENTER_SHORT:
                    if (currentSelection == SelectionPage::DEV) {
                        currentState = MenuState::DEVICE_SELECT;
                        currentDevicePage = DevicePage::D3;
                    } else if (currentSelection == SelectionPage::SET) {
                        currentState = MenuState::SETTINGS_SUBMENU;
                        currentSettingsPage = SettingsPage::CHG;
                    } else if (currentSelection == SelectionPage::AIR) {
                        currentState = MenuState::WAVE2_MENU;
                        currentWave2Page = Wave2MenuPage::PWR;
                    } else {
                        currentState = MenuState::DETAIL;
                    }
                    break;
                 default: break;
            }
            break;

        case MenuState::DETAIL:
            if (input == ButtonInput::BTN_ENTER_SHORT) { // Toggle on Short Press
                flashScreen(cWhite);
                switch(currentSelection) {
                    case SelectionPage::AC: return DisplayAction::TOGGLE_AC;
                    case SelectionPage::DC: return DisplayAction::TOGGLE_DC;
                    case SelectionPage::USB: return DisplayAction::TOGGLE_USB;
                    default: break;
                }
            }
            break;

        case MenuState::WAVE2_MENU:
            {
                bool showSmd = (currentData.wave2.mode == 0 || currentData.wave2.mode == 1);
                switch(input) {
                    case ButtonInput::BTN_UP:
                    case ButtonInput::BTN_DOWN:
                        {
                            int max = showSmd ? 3 : 2;
                            int cur = (int)currentWave2Page;
                            int dir = (input == ButtonInput::BTN_UP) ? -1 : 1;
                            cur = cur + dir;
                            if (cur < 0) cur = max;
                            if (cur > max) cur = 0;
                            currentWave2Page = (Wave2MenuPage)cur;
                        }
                        break;
                    case ButtonInput::BTN_ENTER_SHORT:
                        if (currentWave2Page == Wave2MenuPage::PWR) {
                            currentState = MenuState::EDIT_W2_PWR;
                            tempW2Val = currentData.wave2.powerMode;
                            if (tempW2Val < 1 || tempW2Val > 2) tempW2Val = 2; // Default to OFF
                        } else if (currentWave2Page == Wave2MenuPage::MOD) {
                            currentState = MenuState::EDIT_W2_MOD;
                            tempW2Val = currentData.wave2.mode;
                        } else if (currentWave2Page == Wave2MenuPage::SPD) {
                            currentState = MenuState::EDIT_W2_SPD;
                            tempW2Val = currentData.wave2.fanValue;
                        } else if (currentWave2Page == Wave2MenuPage::SMD) {
                            currentState = MenuState::EDIT_W2_SMD;
                            tempW2Val = currentData.wave2.subMode;
                        }
                        break;
                    default: break;
                }
            }
            break;

        case MenuState::EDIT_W2_PWR:
        case MenuState::EDIT_W2_MOD:
        case MenuState::EDIT_W2_SPD:
        case MenuState::EDIT_W2_SMD:
            switch(input) {
                case ButtonInput::BTN_UP:
                    if (currentState == MenuState::EDIT_W2_PWR) { tempW2Val = (tempW2Val == 1) ? 2 : 1; }
                    if (currentState == MenuState::EDIT_W2_MOD) { tempW2Val++; if(tempW2Val>2) tempW2Val=0; }
                    if (currentState == MenuState::EDIT_W2_SPD) { tempW2Val++; if(tempW2Val>2) tempW2Val=0; }
                    if (currentState == MenuState::EDIT_W2_SMD) { tempW2Val++; if(tempW2Val>3) tempW2Val=0; }
                    break;
                case ButtonInput::BTN_DOWN:
                    if (currentState == MenuState::EDIT_W2_PWR) { tempW2Val = (tempW2Val == 1) ? 2 : 1; }
                    if (currentState == MenuState::EDIT_W2_MOD) { tempW2Val--; if(tempW2Val<0) tempW2Val=2; }
                    if (currentState == MenuState::EDIT_W2_SPD) { tempW2Val--; if(tempW2Val<0) tempW2Val=2; }
                    if (currentState == MenuState::EDIT_W2_SMD) { tempW2Val--; if(tempW2Val<0) tempW2Val=3; }
                    break;
                case ButtonInput::BTN_ENTER_SHORT: // Confirm
                    flashScreen(cWhite);
                    {
                        MenuState prevState = currentState;
                        currentState = MenuState::WAVE2_MENU;
                        if (prevState == MenuState::EDIT_W2_PWR) return DisplayAction::W2_SET_PWR;
                        if (prevState == MenuState::EDIT_W2_MOD) return DisplayAction::W2_SET_MODE;
                        if (prevState == MenuState::EDIT_W2_SPD) return DisplayAction::W2_SET_FAN;
                        if (prevState == MenuState::EDIT_W2_SMD) return DisplayAction::W2_SET_SUB_MODE;
                    }
                    break;
            }
            break;

        case MenuState::SETTINGS_SUBMENU:
            switch(input) {
                case ButtonInput::BTN_UP:
                case ButtonInput::BTN_DOWN:
                    {
                        int idx = (int)currentSettingsPage;
                        if (idx == 0) idx = 2; // Wrap
                        else idx--;
                         if (input == ButtonInput::BTN_DOWN) {
                             if (currentSettingsPage == SettingsPage::CHG) currentSettingsPage = SettingsPage::LIM;
                             else if (currentSettingsPage == SettingsPage::LIM) currentSettingsPage = SettingsPage::OFF;
                             else currentSettingsPage = SettingsPage::CHG;
                         } else {
                             if (currentSettingsPage == SettingsPage::CHG) currentSettingsPage = SettingsPage::OFF;
                             else if (currentSettingsPage == SettingsPage::LIM) currentSettingsPage = SettingsPage::CHG;
                             else currentSettingsPage = SettingsPage::LIM;
                         }
                    }
                    break;
                case ButtonInput::BTN_ENTER_SHORT:
                    if (currentSettingsPage == SettingsPage::CHG) {
                        currentState = MenuState::EDIT_CHG;
                        DeviceType active = getActiveDeviceType();
                        targetDeviceType = active; // Set global target for main loop
                        if (active == DeviceType::DELTA_PRO_3) {
                             tempAcLimit = currentData.deltaPro3.acChargingSpeed;
                             if (tempAcLimit < 400 || tempAcLimit > 2900) tempAcLimit = 400;
                        } else {
                             tempAcLimit = currentData.delta3.acChargingSpeed;
                             if (tempAcLimit < 200 || tempAcLimit > 2900) tempAcLimit = 400; // D3 is 100-1500 but 200 is default min
                        }
                    } else if (currentSettingsPage == SettingsPage::LIM) {
                        currentState = MenuState::LIMITS_SUBMENU;
                        currentLimitsPage = LimitsPage::UP;
                    } else if (currentSettingsPage == SettingsPage::OFF) {
                        return DisplayAction::SYSTEM_OFF;
                    }
                    break;
                 default: break;
            }
            break;

        case MenuState::LIMITS_SUBMENU:
            switch(input) {
                case ButtonInput::BTN_UP:
                case ButtonInput::BTN_DOWN:
                    if (currentLimitsPage == LimitsPage::UP) currentLimitsPage = LimitsPage::DN;
                    else currentLimitsPage = LimitsPage::UP;
                    break;
                case ButtonInput::BTN_ENTER_SHORT:
                    tempMaxChg = currentData.delta3.batteryChargeLimitMax;
                    if (tempMaxChg < 50 || tempMaxChg > 100) tempMaxChg = 100;

                    tempMinDsg = currentData.delta3.batteryChargeLimitMin;
                    if (tempMinDsg < 0 || tempMinDsg > 30) tempMinDsg = 0;

                    if (currentLimitsPage == LimitsPage::UP) {
                        currentState = MenuState::EDIT_SOC_UP;
                    } else {
                        currentState = MenuState::EDIT_SOC_DN;
                    }
                    break;
                default: break;
            }
            break;

        case MenuState::EDIT_CHG:
            switch(input) {
                case ButtonInput::BTN_UP:
                    tempAcLimit += 100;
                    {
                        DeviceType active = targetDeviceType;
                        int max = (active == DeviceType::DELTA_PRO_3) ? 2900 : 1500;
                        if (tempAcLimit > max) tempAcLimit = max;
                    }
                    break;
                case ButtonInput::BTN_DOWN:
                    tempAcLimit -= 100;
                    {
                        DeviceType active = targetDeviceType;
                        int min = (active == DeviceType::DELTA_PRO_3) ? 400 : 100;
                        if (tempAcLimit < min) tempAcLimit = min;
                    }
                    break;
                case ButtonInput::BTN_ENTER_SHORT: // Confirm
                    flashScreen(cWhite);
                    currentState = MenuState::DASHBOARD;
                    return DisplayAction::SET_AC_LIMIT;
                default: break;
            }
            break;

        case MenuState::EDIT_SOC_UP:
            switch(input) {
                case ButtonInput::BTN_UP:
                    tempMaxChg += 10;
                    if (tempMaxChg > 100) tempMaxChg = 100;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempMaxChg -= 10;
                    if (tempMaxChg < 50) tempMaxChg = 50;
                    break;
                case ButtonInput::BTN_ENTER_SHORT: // Confirm
                    flashScreen(cWhite);
                    currentState = MenuState::DASHBOARD;
                    return DisplayAction::SET_SOC_LIMITS;
                default: break;
            }
            break;

        case MenuState::EDIT_SOC_DN:
            switch(input) {
                case ButtonInput::BTN_UP:
                    tempMinDsg += 10;
                    if (tempMinDsg > 30) tempMinDsg = 30;
                    break;
                case ButtonInput::BTN_DOWN:
                    tempMinDsg -= 10;
                    if (tempMinDsg < 0) tempMinDsg = 0;
                    break;
                case ButtonInput::BTN_ENTER_SHORT: // Confirm
                    flashScreen(cWhite);
                    currentState = MenuState::DASHBOARD;
                    return DisplayAction::SET_SOC_LIMITS;
                default: break;
            }
            break;

        case MenuState::DEVICE_SELECT:
            switch(input) {
                case ButtonInput::BTN_UP:
                case ButtonInput::BTN_DOWN:
                    {
                        int idx = (int)currentDevicePage;
                        idx = (idx + 1) % 4; // 4 devices
                        currentDevicePage = (DevicePage)idx;
                    }
                    break;
                case ButtonInput::BTN_ENTER_SHORT:
                    currentState = MenuState::DEVICE_ACTION;
                    currentDeviceAction = DeviceActionPage::CON;
                    if (currentDevicePage == DevicePage::D3) targetDeviceType = DeviceType::DELTA_3;
                    else if (currentDevicePage == DevicePage::W2) targetDeviceType = DeviceType::WAVE_2;
                    else if (currentDevicePage == DevicePage::D3P) targetDeviceType = DeviceType::DELTA_PRO_3;
                    else if (currentDevicePage == DevicePage::CHG) targetDeviceType = DeviceType::ALTERNATOR_CHARGER;
                    break;
                default: break;
            }
            break;

        case MenuState::DEVICE_ACTION:
             switch(input) {
                case ButtonInput::BTN_UP:
                case ButtonInput::BTN_DOWN:
                    if (currentDeviceAction == DeviceActionPage::CON) currentDeviceAction = DeviceActionPage::DIS;
                    else currentDeviceAction = DeviceActionPage::CON;
                    break;
                case ButtonInput::BTN_ENTER_SHORT:
                    if (currentDeviceAction == DeviceActionPage::CON) return DisplayAction::CONNECT_DEVICE;
                    else return DisplayAction::DISCONNECT_DEVICE;
                    break;
                default: break;
             }
             break;
    }

    return DisplayAction::NONE;
}

DisplayAction getPendingAction() {
    DisplayAction act = pendingAction;
    pendingAction = DisplayAction::NONE;
    return act;
}

int getSetAcLimit() { return tempAcLimit; }
int getSetMaxChgSoc() { return tempMaxChg; }
int getSetMinDsgSoc() { return tempMinDsg; }
DeviceType getTargetDeviceType() { return targetDeviceType; }
int getSetW2Val() { return tempW2Val; }
