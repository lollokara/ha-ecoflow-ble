#include "ff.h"
#include <stdio.h>

#if FF_FS_REENTRANT
#include "FreeRTOS.h"
#include "semphr.h"

// Note: FatFs expects int return 1 on success for mutex_create
// and int return 1 on success for mutex_take

static SemaphoreHandle_t Mutex[FF_VOLUMES + 1]; // +1 for system mutex (FF_VOLUMES)

int ff_mutex_create (int vol)
{
    if (vol > FF_VOLUMES) return 0;

    // Check if already created
    if (Mutex[vol] != NULL) return 1;

    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        printf("FF: Mutex Create Failed (OOM?)\n");
        return 0;
    }
    Mutex[vol] = mutex;
    return 1;
}

void ff_mutex_delete (int vol)
{
    if (vol > FF_VOLUMES || !Mutex[vol]) return;
    vSemaphoreDelete(Mutex[vol]);
    Mutex[vol] = NULL;
}

int ff_mutex_take (int vol)
{
    if (vol > FF_VOLUMES || !Mutex[vol]) {
        printf("FF: Mutex Take NULL (Vol %d)\n", vol);
        return 0;
    }
    if (xSemaphoreTake(Mutex[vol], FF_FS_TIMEOUT) != pdTRUE) {
        printf("FF: Mutex Take Timeout\n");
        return 0;
    }
    return 1;
}

void ff_mutex_give (int vol)
{
    if (vol <= FF_VOLUMES && Mutex[vol]) xSemaphoreGive(Mutex[vol]);
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
