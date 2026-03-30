# ≡ DATA REFERENCE // DELTA 3

> **DEVICE ID:** `0x01`
> **PROTOCOL:** V3 (Protobuf)
> **ACCESS LEVEL:** FULL

The Delta 3 exposes a comprehensive set of telemetry data via the `pd335_sys_DisplayPropertyUpload` protobuf message.

## ≡ TELEMETRY FIELDS

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **batteryLevel** | `float` | % | State of Charge (SOC) (0-100%). |
| **inputPower** | `float` | W | Total Input Power (AC + DC/Solar). |
| **outputPower** | `float` | W | Total Output Power (AC + DC + USB). |
| **acInputPower** | `float` | W | Grid AC Input Power. |
| **acOutputPower** | `float` | W | Inverter Output Power. |
| **dc12vOutputPower** | `float` | W | Car Lighter Port Output (12V). |
| **dcPortInputPower** | `float` | W | Input from XT60 Port (Solar/Car). |
| **usbOutputPower** | `float` | W | Aggregate USB-A and USB-C Output. |
| **acChargingSpeed** | `int32` | W | Configured AC Charging Limit. |
| **batteryChargeLimitMin** | `int32` | % | Minimum SOC (Discharge Limit). |
| **batteryChargeLimitMax** | `int32` | % | Maximum SOC (Charge Limit). |
| **cellTemperature** | `int32` | °C | Maximum Cell Temperature. |
| **energyBackup** | `bool` | - | Energy Backup Mode Enabled. |

## ≡ STATE FLAGS

| Flag | Description |
| :--- | :--- |
| **acPorts** | AC Inverter is Active (On/Off). |
| **dc12vPort** | 12V DC Output is Active (On/Off). |
| **usbOn** | USB Ports are providing power. |
| **pluggedInAc** | AC Grid Cable connected. |

---

> *Reference: `pd335_sys.pb.c` / `Delta3DataStruct`*
