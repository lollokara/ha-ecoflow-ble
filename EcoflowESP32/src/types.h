#ifndef ECOFLOW_TYPES_H
#define ECOFLOW_TYPES_H

#include <string>
#include <vector>
#include <Arduino.h>

// --- Device Types ---
enum class DeviceType {
    UNKNOWN = 0,
    DELTA_3 = 1,
    DELTA_PRO_3 = 2,
    WAVE_2 = 3,
    ALTERNATOR_CHARGER = 4
};

// --- Display Enums ---
enum class DisplayAction {
    NONE,
    CONNECT_D3,
    CONNECT_W2,
    CONNECT_D3P,
    CONNECT_ALT,
    SET_AC_LIMIT,
    SET_MAX_CHG,
    SET_MIN_DSG,
    SET_W2_TEMP,
    SET_W2_FAN,
    FORGET_DEV,
    TOGGLE_AC,
    TOGGLE_DC,
    TOGGLE_USB,
    W2_SET_PWR,
    W2_SET_MODE,
    W2_SET_FAN,
    W2_SET_SUB_MODE,
    SYSTEM_OFF,
    SET_SOC_LIMITS,
    CONNECT_DEVICE,
    DISCONNECT_DEVICE
};

enum class ButtonInput {
    NONE,
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER,
    BTN_BACK,
    BTN_ENTER_SHORT,
    BTN_ENTER_HOLD
};

// --- Data Structures ---

struct Delta3Data {
    float batteryLevel;
    float inputPower;
    float outputPower;
    float acInputPower;
    float acOutputPower;
    float dcPortInputPower;
    float solarInputPower;
    float batteryInputPower;
    float batteryOutputPower;
    float dc12vOutputPower;
    float usbcOutputPower;
    float usbc2OutputPower;
    float usbaOutputPower;
    float usba2OutputPower;
    int batteryChargeLimitMin;
    int batteryChargeLimitMax;
    int acChargingSpeed;
    int maxAcChargingPower; // Added
    int dcPortState; // 0=None, 1=Car, 2=Solar
    int energyBackup;
    int energyBackupBatteryLevel;
    int cellTemperature;
    bool acOn;
    bool dcOn;
    bool usbOn;
    bool pluggedInAc;
    bool dc12vPort;
    bool acPorts;
};

struct DeltaPro3Data {
    float batteryLevel;
    float batteryLevelMain;
    float acInputPower;
    float acLvOutputPower;
    float acHvOutputPower;
    float inputPower;
    float outputPower;
    float dc12vOutputPower;
    float dcLvInputPower;
    float dcHvInputPower;
    int dcLvInputState; // 0=None, 1=Car, 2=Solar
    int dcHvInputState; // 0=None, 1=Car, 2=Solar
    float solarLvPower; // Derived
    float solarHvPower; // Derived
    float usbaOutputPower;
    float usba2OutputPower;
    float usbcOutputPower;
    float usbc2OutputPower;
    int acChargingSpeed;
    int maxAcChargingPower;
    int energyBackup;
    int energyBackupBatteryLevel;
    int batteryChargeLimitMin;
    int batteryChargeLimitMax;
    int cellTemperature;
    bool dc12vPort;
    bool acLvPort;
    bool acHvPort;
    bool gfiMode;
    bool pluggedInAc; // Added to match EcoflowData.h

    // New fields
    float expansion1_power;
    float expansion2_power;
    int ac_in_status;
    float bms_batt_soh;
    uint32_t bms_dsg_rem_time;
    uint32_t bms_chg_rem_time;
};

struct Wave2Data {
    int mode; // 0=Cool, 1=Heat, 2=Fan
    int subMode; // 0=Max, 1=Sleep, 2=Eco, 3=Manual
    int setTemp;
    int fanValue;
    float envTemp;
    float outLetTemp;
    int batSoc;
    int batChgStatus; // 0=Idle, 1=Chg, 2=Dsg
    uint32_t batChgRemainTime;
    uint32_t batDsgRemainTime;
    uint32_t remainingTime; // Derived
    int batPwrWatt;
    int mpptPwrWatt;
    int psdrPwrWatt;
    int tempSys;
    int displayIdleTime;
    int displayIdleMode;
    int timeEn;
    int timeSetVal;
    int timeRemainVal;
    int beepEnable;
    uint32_t errCode;
    int refEn;
    int bmsPid;
    int wteFthEn;
    int tempDisplay; // 0=C, 1=F
    int powerMode; // 1=ON, 2=OFF
    int powerSrc;
    int mpptWork;
    int bmsErr;
    int rgbState; // 0=Flow, 1=Follow, 2=Breath, 3=Night, 4=Plain
    int waterValue; // 0=No, 1=Low, 2=Med, 3=High
    int bmsBoundFlag;
    int bmsUndervoltage;
    int ver;
};

struct AlternatorChargerData {
    float batteryLevel;
    int batteryTemperature;
    float dcPower;
    float carBatteryVoltage;
    float startVoltage;
    int startVoltageMin; // Added
    int startVoltageMax; // Added
    int chargerMode; // 0=Charge, 1=Reverse
    bool chargerOpen;
    int powerLimit;
    int powerMax;
    float reverseChargingCurrentLimit;
    float chargingCurrentLimit;
    float reverseChargingCurrentMax;
    float chargingCurrentMax;
};

struct EcoflowData {
    bool isConnected; // Added
    Delta3Data delta3;
    DeltaPro3Data deltaPro3;
    Wave2Data wave2;
    AlternatorChargerData alternatorCharger;
};

#endif // ECOFLOW_TYPES_H
