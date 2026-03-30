# ≡ DATA REFERENCE // ALTERNATOR CHARGER

> **DEVICE ID:** `0x04`
> **PROTOCOL:** V3 (Protobuf)
> **ACCESS LEVEL:** BETA

The Alternator Charger (Series 800W) exposes telemetry via `dc009_apl_comm.proto`.

## ≡ TELEMETRY FIELDS

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **batteryLevel** | `float` | % | Connected Battery State of Charge. |
| **batteryTemperature** | `float` | °C | Connected Battery Temperature. |
| **dcPower** | `float` | W | Bi-directional DC Power Flow. |
| **carBatteryVoltage** | `float` | V | Voltage of the Vehicle Battery (Input). |
| **startVoltage** | `float` | V | Configured Start Voltage (Engine Running Detect). |
| **chargerMode** | `Enum` | - | Operation Mode (See Below). |
| **chargerOpen** | `bool` | - | Master Switch State (On/Off). |
| **powerLimit** | `int` | W | User Configured Charging Limit. |
| **powerMax** | `int` | W | Maximum Hardware Capability. |
| **chargingCurrentLimit** | `float` | A | Current Limit for charging Device Battery. |
| **reverseChargingCurrentLimit** | `float` | A | Current Limit for Reverse Charging (Vehicle). |

## ≡ ENUMS

### Charger Mode
| Value | Mode | Description |
| :--- | :--- | :--- |
| `0` | **IDLE** | Standby mode. |
| `1` | **DRIVING_CHG** | Charging from Alternator (Drive). |
| `2` | **BAT_MAINTENANCE** | Maintaining Vehicle Battery. |
| `3` | **PARKING_CHG** | Reverse Charging (Parking). |

---

> *Reference: `dc009_apl_comm.proto` / `AlternatorChargerDataStruct`*
