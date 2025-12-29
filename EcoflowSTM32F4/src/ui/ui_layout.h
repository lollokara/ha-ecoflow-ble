#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecoflow_protocol.h"

void UI_Init(void);
void UI_UpdateBattery(BatteryStatus *status);
void UI_UpdateTemperature(Temperature *temp);
void UI_UpdateConnection(UartConnectionState *conn);

#ifdef __cplusplus
}
#endif

#endif
