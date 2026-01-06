#include "ff.h"

#if FF_FS_REENTRANT
#include "FreeRTOS.h"
#include "semphr.h"

// Note: FatFs expects int return 1 on success for mutex_create
// and int return 1 on success for mutex_take

int ff_mutex_create (int vol)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    return (int)mutex;
}

void ff_mutex_delete (int vol)
{
    if (vol) vSemaphoreDelete((SemaphoreHandle_t)vol);
}

int ff_mutex_take (int vol)
{
    if (!vol) return 0;
    return xSemaphoreTake((SemaphoreHandle_t)vol, FF_FS_TIMEOUT) == pdTRUE;
}

void ff_mutex_give (int vol)
{
    if (vol) xSemaphoreGive((SemaphoreHandle_t)vol);
}
#endif

#if FF_USE_LFN == 3
#include "FreeRTOS.h"

void* ff_memalloc (UINT msize)
{
    return pvPortMalloc(msize);
}

void ff_memfree (void* mblock)
{
    vPortFree(mblock);
}
#endif
