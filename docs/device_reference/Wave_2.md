# ≡ DATA REFERENCE // WAVE 2

> **DEVICE ID:** `0x03`
> **PROTOCOL:** V2 (Binary)
> **ACCESS LEVEL:** PARTIAL

The Wave 2 uses a legacy binary struct protocol. Telemetry is received in a packed byte array.

## ≡ TELEMETRY FIELDS

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **mode** | `int` | Enum | Main Mode: 0=Cool, 1=Heat, 2=Fan. |
| **subMode** | `int` | Enum | 0=Max, 1=Sleep, 2=Eco, 3=Auto. |
| **setTemp** | `int` | °C | Target Temperature Setpoint. |
| **envTemp** | `float` | °C | Ambient Environment Temperature. |
| **outLetTemp** | `float` | °C | Outlet Air Temperature. |
| **fanValue** | `int` | RPM | Internal Fan Speed (Approx). |
| **batSoc** | `int` | % | Add-on Battery State of Charge. |
| **batPwrWatt** | `int` | W | Battery Power Flow (+/-). |
| **mpptPwrWatt** | `int` | W | Solar Input Power. |
| **psdrPwrWatt** | `int` | W | Power Supply Unit Power. |
| **waterValue** | `int` | % | Water Level (Drain status). |
| **remainingTime** | `int` | Mins | Time to Empty/Full. |

## ≡ CONTROL PARAMETERS

| Parameter | ID | Values |
| :--- | :--- | :--- |
| **TEMP** | `0x01` | 16-30 (°C) |
| **MODE** | `0x02` | 0 (Cool), 1 (Heat), 2 (Fan) |
| **SUBMODE** | `0x03` | 0 (Max), 1 (Sleep), 2 (Eco) |
| **FAN** | `0x04` | 0-100 (Speed) |

---

> *Reference: `EcoflowDataParser.cpp` / `Wave2DataStruct`*
