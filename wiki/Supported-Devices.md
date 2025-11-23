# Supported Devices

This library has been developed and tested with a primary focus on the Delta 3, but it also includes partial support for other devices based on the same underlying BLE protocol.

## Compatibility List

| Device Model      | Protocol Version | Status      | Notes                                                              |
|-------------------|------------------|-------------|--------------------------------------------------------------------|
| **Delta 3**       | `3`              | ✅ Full      | This is the primary target device. All features are supported.       |
| **Wave 2**        | `2`              | ✅ Full      | All core monitoring and control features are implemented.          |
| **Delta 3 Pro**   | `3`              | ✅ Full      | All core monitoring and control features are implemented.          |
| **Alternator Charger** | `3`         | ✅ Full      | All core monitoring and control features are implemented.          |
| *Other Devices*   | `?`              | ❔ Unknown   | Other devices may work if they use a similar protocol version.     |

## Protocol Versions

The `protocolVersion` is a key parameter in the `begin()` function. It determines device-specific behaviors, such as the BLE destination address for commands.

-   **Version 3 (Delta 3, Delta 3 Pro, Alt Charger):** Uses destination address `0x02` for most commands.
-   **Version 2 (Wave 2):** Uses destination address `0x42`.

If you are attempting to use this library with an unsupported device, experimenting with the protocol version is a good first step.
