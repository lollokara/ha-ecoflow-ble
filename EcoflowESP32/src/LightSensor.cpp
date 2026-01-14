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
    if (millis() - _lastUpdate < 50) return; // 20Hz sampling for better smoothing
    _lastUpdate = millis();

    int val = analogRead(_pin);

    // Outlier Rejection: If val deviates significantly from average (and average is established), ignore it
    // But allow it to adapt if sustained.
    // Use a simple median-ish approach or just stronger smoothing?
    // Let's implement a weighted check.

    // First run?
    if (_total == 0 && _average == 0) {
         for (int i = 0; i < WINDOW_SIZE; i++) _readings[i] = val;
         _total = val * WINDOW_SIZE;
         _average = val;
    }

    // Spike filter: If deviation > 500, clamp it or ignore it.
    // However, turning on a light IS a spike.
    // We want to filter *noise*. Noise is usually high frequency.
    // Let's rely on the moving average but increase window size?
    // Or ignore one-off spikes.

    static int consecutive_outliers = 0;
    if (abs(val - _average) > 800) {
        consecutive_outliers++;
        if (consecutive_outliers < 5) {
            return; // Ignore first few spikes
        }
    } else {
        consecutive_outliers = 0;
    }

    _total = _total - _readings[_readIndex];
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
