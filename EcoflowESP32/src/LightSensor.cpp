#include "LightSensor.h"
#include <esp_log.h>

static const char* TAG = "LightSensor";

LightSensor& LightSensor::getInstance() {
    static LightSensor instance;
    return instance;
}

LightSensor::LightSensor() {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        _readings[i] = 0;
    }
}

void LightSensor::begin() {
    _prefs.begin("light_cfg", false);
    _minADC = _prefs.getInt("min", 0);
    _maxADC = _prefs.getInt("max", 4095);
    _prefs.end();

    // Initialize readings with current value to avoid ramp-up
    pinMode(_pin, INPUT);
    //analogReadResolution(12); // Default is 12 for ESP32
    //analogSetAttenuation(ADC_11db); // Default

    int initial = analogRead(_pin);
    for (int i = 0; i < WINDOW_SIZE; i++) {
        _readings[i] = initial;
    }
    _total = initial * WINDOW_SIZE;
    _average = initial;

    ESP_LOGI(TAG, "Initialized: Min=%d, Max=%d, Curr=%d", _minADC, _maxADC, initial);
}

void LightSensor::update() {
    unsigned long now = millis();
    if (now - _lastUpdate < 50) return; // 20Hz sampling
    _lastUpdate = now;

    int val = analogRead(_pin);

    _total = _total - _readings[_readIndex];
    _readings[_readIndex] = val;
    _total = _total + _readings[_readIndex];
    _readIndex = (_readIndex + 1) % WINDOW_SIZE;

    _average = _total / WINDOW_SIZE;
}

int LightSensor::getRaw() const {
    return _average;
}

int LightSensor::getMin() const {
    return _minADC;
}

int LightSensor::getMax() const {
    return _maxADC;
}

void LightSensor::setCalibration(int min, int max) {
    if (min < 0) min = 0;
    if (max > 4095) max = 4095;
    if (min >= max) min = max - 1; // Basic sanity check

    _minADC = min;
    _maxADC = max;

    _prefs.begin("light_cfg", false);
    _prefs.putInt("min", _minADC);
    _prefs.putInt("max", _maxADC);
    _prefs.end();

    ESP_LOGI(TAG, "Calibration Saved: Min=%d, Max=%d", _minADC, _maxADC);
}
