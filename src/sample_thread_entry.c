#include "sample_thread.h"

#include "FreeRTOS.h"
#include "task.h"
#include "os_taskprio.h"
#include "lsa_cfg.h"
#include "pniousrd.h"
#include "os.h"
#include "ether_config.h"

void vApplicationMallocFailedHook(void);
void vApplicationStackOverflowHook(TaskHandle_t , char *);
void vApplicationIdleHook(void);
void vApplicationTickHook(void);

extern void MainAppl(void);
LSA_UINT32 TskId_Main;

/* Start Task entry function */
/* pvParameters contains TaskHandle_t */
void sample_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    ether_buffers_init();

    //---------------------------------
    // Initialize FSP modules
    //----------------------------------
    R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);

    //---------------------------------
    // Initialize PROFINET stack
    //---------------------------------
    PNIO_init();

    //---------------------------------
    // Create MainAppl() task
    //---------------------------------
    OsCreateThread(MainAppl, (PNIO_UINT8*)"MainAppl", TASK_PRIO_MAIN, &TskId_Main);
    OsCreateMsgQueue(TskId_Main);
    OsStartThread(TskId_Main);

    while(1)
    {
        vTaskSuspend(NULL);
    }
}


void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    ( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    #if( mainSELECTED_APPLICATION == 1 )
    {
        /* Only the comprehensive demo actually uses the tick hook. */
        extern void vFullDemoTickHook( void );
        vFullDemoTickHook();
    }
    #endif
}
/*-----------------------------------------------------------*/
