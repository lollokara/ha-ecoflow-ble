# EcoflowESP32

An Arduino library for controlling Ecoflow devices with an ESP32.

## Features

- Scan for and connect to Ecoflow devices over BLE.
- Turn AC, DC (12V), and USB outputs on and off.
- Read battery level, input power, and output power.

## Installation

### Arduino IDE

1.  Download the latest release from the [releases page](https://github.com/your-username/EcoflowESP32/releases).
2.  In the Arduino IDE, go to `Sketch` > `Include Library` > `Add .ZIP Library...` and select the downloaded file.

### PlatformIO

Add the following to your `platformio.ini` file:

```ini
lib_deps =
  your-username/EcoflowESP32
```

## Usage

See the `BareMinimum` and `Comprehensive` examples in the `examples` directory.
