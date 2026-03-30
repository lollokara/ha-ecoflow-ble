# ≡ DATA REFERENCE // DELTA PRO 3

> **DEVICE ID:** `0x02`
> **PROTOCOL:** V3 (Protobuf)
> **ACCESS LEVEL:** BETA

The Delta Pro 3 introduces split-phase and dual-input capabilities.

## ≡ TELEMETRY FIELDS

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **batteryLevel** | `float` | % | State of Charge. |
| **acLvOutputPower** | `float` | W | 120V (Low Voltage) Output. |
| **acHvOutputPower** | `float` | W | 240V (High Voltage) Output. |
| **dc12vOutputPower** | `float` | W | 12V Anderson/Cigarette Output. |
| **solarLvPower** | `float` | W | Low Voltage PV Input. |
| **solarHvPower** | `float` | W | High Voltage PV Input. |
| **inputPower** | `float` | W | Total System Input. |
| **outputPower** | `float` | W | Total System Output. |
| **gfiMode** | `bool` | - | Ground Fault Interrupt Status. |

## ≡ STATE FLAGS

| Flag | Description |
| :--- | :--- |
| **acLvPort** | 120V Inverter Active. |
| **acHvPort** | 240V Inverter Active. |
| **dc12vPort** | 12V Output Active. |

---

> *Reference: `mr521.pb.c` / `DeltaPro3DataStruct`*
