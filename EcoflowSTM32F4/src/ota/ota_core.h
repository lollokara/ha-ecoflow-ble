#ifndef OTA_CORE_H
#define OTA_CORE_H

#include <stdint.h>

void OtaCore_Init(void);
void OtaCore_HandleCmd(uint8_t cmd_id, uint8_t *data, uint32_t len);

#endif // OTA_CORE_H
