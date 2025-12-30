#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>
#include <Preferences.h>

class LightSensor {
public:
    static LightSensor& getInstance();

    void begin();
    void update();

    int getRaw() const; // Current filtered value
    int getMin() const;
    int getMax() const;
    uint8_t getBrightness() const; // Returns 0-100% mapped from raw range

    void setCalibration(int min, int max);

private:
    LightSensor();

    // Config
    int _pin = 1; // GPIO 1
    int _minADC = 0;
    int _maxADC = 4095;

    // Filtering
    static const int WINDOW_SIZE = 20;
    int _readings[WINDOW_SIZE];
    int _readIndex = 0;
    long _total = 0;
    int _average = 0;

    Preferences _prefs;
    unsigned long _lastUpdate = 0;
};

#endif
