# Troubleshooting Guide

This page covers common issues you might encounter while using the EcoflowESP32 library and how to resolve them.

## Connection Timeouts

This is the most common issue, where the device connects but fails to complete the authentication handshake, eventually timing out.

#### Symptom:

The device connects, but the state gets stuck at `PUBLIC_KEY_EXCHANGE` or `REQUESTING_SESSION_KEY` and then transitions to `ERROR_TIMEOUT`.

#### Possible Causes & Solutions:

1.  **Incorrect Credentials (`Credentials.h`)**
    -   **Solution:** Double-check that your `ECOFLOW_USER_ID`, `ECOFLOW_DEVICE_SN`, and `ECOFLOW_KEYDATA` are 100% correct. An error in any of these will cause the handshake to fail silently.

2.  **Wrong Protocol Version**
    -   **Solution:** Ensure you are using the correct protocol version for your device in the `begin()` call. For example, the Delta 3 uses version 3, while the Wave 2 uses version 2. See the [[Supported Devices|Supported-Devices]] page for more details.

3.  **BLE Signal Interference**
    -   **Solution:** Bluetooth signals can be unreliable. Try moving the ESP32 closer to the EcoFlow device. Keep them away from other sources of 2.4GHz interference, such as Wi-Fi routers or microwave ovens.

4.  **Device Already Connected**
    -   **Solution:** The EcoFlow device can typically only handle one active BLE connection at a time. Make sure you have disconnected the device from the official EcoFlow app on your phone and any other BLE devices.

## Compilation Errors

#### Symptom:

The project fails to compile in PlatformIO.

#### Possible Causes & Solutions:

1.  **Missing `Credentials.h`**
    -   **Solution:** You must create the `src/Credentials.h` file yourself. It is not included in the repository. See the [[Usage & Examples|Usage-&-Examples]] page for the required format.

2.  **Missing Nanopb Files**
    -   **Solution:** The `.pb.c` and `.pb.h` files are generated from the `.proto` files. If they are missing, you may need to ensure the Nanopb generator has been run correctly within the project's dependencies. A clean build in PlatformIO (`pio run -t clean`) often resolves this.

## Device Not Found During Scan

#### Symptom:

The `DeviceManager` scans but never finds the target device.

#### Possible Causes & Solutions:

1.  **Device Not in Pairing Mode**
    -   **Solution:** Ensure your EcoFlow device's Bluetooth is on and discoverable. You may need to press a button on the device to enable pairing.

2.  **Incorrect Serial Number**
    -   **Solution:** The scanner looks for a BLE advertisement that contains the device's serial number. Verify that the `ECOFLOW_DEVICE_SN` in your `Credentials.h` is correct.
