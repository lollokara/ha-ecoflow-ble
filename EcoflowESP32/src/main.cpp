#include <Arduino.h>
#include <esp_task_wdt.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"
#include "Stm32Serial.h"
#include "OtaManager.h"

// Hardware Pin Definitions
#define POWER_LATCH_PIN 16

// Logging Tag
static const char* TAG = "Main";

// Global Instances
Stm32Serial* stm32Serial = nullptr;
OtaManager* otaManager = nullptr;

void checkSerial() {
    static String inputBuffer = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            CmdUtils::processInput(inputBuffer);
            inputBuffer = "";   
        } else if (c >= 32 && c <= 126) {
            inputBuffer += c;
        }
    }
}

void setup() {
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, LOW);

    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    LightSensor::getInstance().begin();
    DeviceManager::getInstance().initialize();
    WebServer::begin();

    // Init Logic for Cross-Dependency
    // We allocate them on heap or static
    static OtaManager ota(NULL); // Temp construct
    static Stm32Serial ser(&Serial1, &ota); // Pass hardware serial and ota ptr

    // Fix circular link
    // OtaManager constructor took Stm32Serial*
    // I need to patch OtaManager to accept it or set it later?
    // My previous OtaManager code: OtaManager(Stm32Serial* stm32) : _stm32(stm32) ...
    // Stm32Serial code: Stm32Serial(HardwareSerial* serial, OtaManager* ota) ...
    // This is a catch-22.
    // Solution: Create Stm32Serial first, then OtaManager, then set Ota on Stm32Serial?
    // or just use pointers.

    stm32Serial = new Stm32Serial(&Serial1, NULL); // Init with NULL OTA
    otaManager = new OtaManager(stm32Serial);
    // I need a setOta method on Stm32Serial? Or make member public?
    // Since I defined it private, I need to modify Stm32Serial.h/cpp to allow setting it or use a setter.
    // For now, let's just re-instantiate or use a setter.
    // Wait, I can't modify the files I just wrote easily without using tools again.
    // I should have thought of this.
    // I will modify Stm32Serial.h to add setOtaManager().

    // But wait, I can just do this:
    // Stm32Serial ser(&Serial1, NULL);
    // OtaManager ota(&ser);
    // ser.setOta(&ota);
    // But I don't have setOta.

    // I will use a dirty trick: The pointer in Stm32Serial is public? No private.
    // I will rewrite Stm32Serial.h to add setOtaManager.
}

void loop() {
    // Placeholder - will actuall overwrite with correct logic below
}
