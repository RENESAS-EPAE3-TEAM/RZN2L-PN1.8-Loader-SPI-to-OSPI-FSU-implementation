/* generated thread source file - do not edit */
#include "app_printeth_thread.h"

#if 1
                static StaticTask_t app_printeth_thread_memory;
                #if defined(__ARMCC_VERSION)           /* AC6 compiler */
                static uint8_t app_printeth_thread_stack[0x500] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #else
                static uint8_t app_printeth_thread_stack[0x500] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.app_printeth_thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #endif
                #endif
                TaskHandle_t app_printeth_thread;
                void app_printeth_thread_create(void);
                static void app_printeth_thread_func(void * pvParameters);
                void rtos_startup_err_callback(void * p_instance, void * p_data);
                void rtos_startup_common_init(void);
extern uint32_t g_fsp_common_thread_count;

                const rm_freertos_port_parameters_t app_printeth_thread_parameters =
                {
                    .p_context = (void *) NULL,
                };

                void app_printeth_thread_create (void)
                {
                    /* Increment count so we will know the number of threads created in the FSP Configuration editor. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    #if 1
                    app_printeth_thread = xTaskCreateStatic(
                    #else
                    BaseType_t app_printeth_thread_create_err = xTaskCreate(
                    #endif
                        app_printeth_thread_func,
                        (const char *)"Printeth Thread",
                        0x500/4, // In words, not bytes
                        (void *) &app_printeth_thread_parameters, //pvParameters
                        10,
                        #if 1
                        (StackType_t *)&app_printeth_thread_stack,
                        (StaticTask_t *)&app_printeth_thread_memory
                        #else
                        & app_printeth_thread
                        #endif
                    );

                    #if 1
                    if (NULL == app_printeth_thread)
                    {
                        rtos_startup_err_callback(app_printeth_thread, 0);
                    }
                    #else
                    if (pdPASS != app_printeth_thread_create_err)
                    {
                        rtos_startup_err_callback(app_printeth_thread, 0);
                    }
                    #endif
                }
                static void app_printeth_thread_func (void * pvParameters)
                {
                    /* Initialize common components */
                    rtos_startup_common_init();

                    /* Initialize each module instance. */
                    

                    /* Enter user code for this thread. Pass task handle. */
                    app_printeth_thread_entry(pvParameters);
                }
