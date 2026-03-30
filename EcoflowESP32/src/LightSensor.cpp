#include "LightSensor.h"

LightSensor& LightSensor::getInstance() {
    static LightSensor instance;
    return instance;
}

LightSensor::LightSensor() {
    for (int i = 0; i < WINDOW_SIZE; i++) _readings[i] = 0;
}

void LightSensor::begin() {
    // Explicitly set ADC resolution and attenuation for ESP32-S3
    analogReadResolution(12);
    // ADC_11db is deprecated in newer cores but usually maps to 11dB (0-3.3V)
    // On ESP32-S3, this is the default full range.
    analogSetAttenuation(ADC_11db);

    pinMode(_pin, INPUT);
    _prefs.begin("lightsensor", false);
    _minADC = _prefs.getInt("min", 0);
    _maxADC = _prefs.getInt("max", 4095);
}

void LightSensor::update() {
    if (millis() - _lastUpdate < 100) return; // 10Hz sampling
    _lastUpdate = millis();

    // Subtract the last reading
    _total = _total - _readings[_readIndex];

    // Read new value
    int val = analogRead(_pin);

    // Simple outlier filter: If value is 0 or 4095, only accept it if it persists (debounce)
    // But averaging handles this mostly.
    // However, if we get random spikes, we can clamp them.
    // Let's just trust the average for now, but ensure initialization doesn't start with 0s if it's bright.

    _readings[_readIndex] = val;
    _total = _total + _readings[_readIndex];
    _readIndex = (_readIndex + 1);
    if (_readIndex >= WINDOW_SIZE) _readIndex = 0;

    _average = _total / WINDOW_SIZE;
}

int LightSensor::getRaw() const {
    return _average;
}

uint8_t LightSensor::getBrightnessPercent() const {
    // Map average ADC to 10-100%
    // Inverted Logic based on user feedback (High ADC = Dark, Low ADC = Bright)
    // ADC 0 (Bright) -> 100%
    // ADC 4095 (Dark) -> 10%
    int val = _average;
    if (val < _minADC) val = _minADC;
    if (val > _maxADC) val = _maxADC;

    int pct = map(val, _minADC, _maxADC, 100, 10);
    return (uint8_t)constrain(pct, 10, 100);
}

int LightSensor::getMin() const { return _minADC; }
int LightSensor::getMax() const { return _maxADC; }

void LightSensor::setCalibration(int min, int max) {
    _minADC = min;
    _maxADC = max;
    _prefs.putInt("min", min);
    _prefs.putInt("max", max);
}
