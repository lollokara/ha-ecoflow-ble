#include "ff.h"

#if FF_FS_REENTRANT
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t FF_Mutexes[FF_VOLUMES];

int ff_mutex_create (int vol)
{
    if (vol >= FF_VOLUMES) return 0;
    FF_Mutexes[vol] = xSemaphoreCreateMutex();
    return (FF_Mutexes[vol] != NULL) ? 1 : 0;
}

void ff_mutex_delete (int vol)
{
    if (vol < FF_VOLUMES && FF_Mutexes[vol]) {
        vSemaphoreDelete(FF_Mutexes[vol]);
        FF_Mutexes[vol] = NULL;
    }
}

int ff_mutex_take (int vol)
{
    if (vol < FF_VOLUMES && FF_Mutexes[vol]) {
        return xSemaphoreTake(FF_Mutexes[vol], FF_FS_TIMEOUT) == pdTRUE;
    }
    return 0;
}

void ff_mutex_give (int vol)
{
    if (vol < FF_VOLUMES && FF_Mutexes[vol]) {
        xSemaphoreGive(FF_Mutexes[vol]);
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
