#include "LightSensor.h"

LightSensor& LightSensor::getInstance() {
    static LightSensor instance;
    return instance;
}

LightSensor::LightSensor() {
    for (int i = 0; i < WINDOW_SIZE; i++) _readings[i] = 0;
}

void LightSensor::begin() {
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
    // Invert because typically LDR pullup means lower value = brighter light?
    // Or depends on wiring. Standard LDR circuit:
    // VCC -> LDR -> Pin -> R -> GND ==> Bright = High V (High ADC)
    // VCC -> R -> Pin -> LDR -> GND ==> Bright = Low V (Low ADC)
    // Assuming Bright = High ADC for now, or calibratable.

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
    // ADC 0 (Dark) -> 10%
    // ADC 4095 (Bright) -> 100%
    int val = _average;
    if (val < _minADC) val = _minADC;
    if (val > _maxADC) val = _maxADC;

    int pct = map(val, _minADC, _maxADC, 10, 100);
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
