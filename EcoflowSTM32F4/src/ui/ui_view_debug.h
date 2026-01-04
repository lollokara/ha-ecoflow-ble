#ifndef UI_VIEW_DEBUG_H
#define UI_VIEW_DEBUG_H

#include "ecoflow_protocol.h"

void UI_CreateDebugView(void);
void UI_UpdateDebugInfo(DebugInfo* info);
void UI_Debug_PeriodicRefresh(void);

#endif // UI_VIEW_DEBUG_H
