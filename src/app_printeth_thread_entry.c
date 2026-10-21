#include "app_printeth_thread.h"
/* Printeth Thread entry function */
/* pvParameters contains TaskHandle_t */
void app_printeth_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

#ifdef PrintfDebug
    extern QueueHandle_t  printeth_queue;
    extern void app_printeth_thread_proc (void);

    printeth_queue = xQueueCreate(10, 64);

    app_printeth_thread_proc();
#endif
    vTaskSuspend(NULL);
}
