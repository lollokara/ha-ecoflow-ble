#ifndef SD_CARD_H
#define SD_CARD_H

#include "main.h"
#include "ff.h"

// SD Card Handle
extern SD_HandleTypeDef hsd;

// Initialization
void MX_SDIO_SD_Init(void);
int SD_Mount(void);
void SD_Unmount(void);

// Formatting
int SD_Format(void);

// Space Info
void SD_GetSpace(uint32_t *total, uint32_t *free);

#endif
