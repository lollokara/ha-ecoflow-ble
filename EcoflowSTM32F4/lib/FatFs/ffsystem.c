#include "ff.h"

#if FF_FS_REENTRANT
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t Mutex[FF_VOLUMES];

int ff_mutex_create (int vol)
{
    if (vol >= FF_VOLUMES) return 0;

    if (Mutex[vol] == NULL) {
        Mutex[vol] = xSemaphoreCreateMutex();
    }
    return (Mutex[vol] != NULL);
}

void ff_mutex_delete (int vol)
{
    if (vol < FF_VOLUMES && Mutex[vol]) {
        vSemaphoreDelete(Mutex[vol]);
        Mutex[vol] = NULL;
    }
}

int ff_mutex_take (int vol)
{
    if (vol >= FF_VOLUMES || !Mutex[vol]) return 0;
    return xSemaphoreTake(Mutex[vol], FF_FS_TIMEOUT) == pdTRUE;
}

void ff_mutex_give (int vol)
{
    if (vol < FF_VOLUMES && Mutex[vol]) {
        xSemaphoreGive(Mutex[vol]);
    }
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
