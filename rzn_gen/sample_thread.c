/* generated thread source file - do not edit */
#include "sample_thread.h"

#if 1
                static StaticTask_t sample_thread_memory;
                #if defined(__ARMCC_VERSION)           /* AC6 compiler */
                static uint8_t sample_thread_stack[0x500] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #else
                static uint8_t sample_thread_stack[0x500] BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".stack.sample_thread") BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
                #endif
                #endif
                TaskHandle_t sample_thread;
                void sample_thread_create(void);
                static void sample_thread_func(void * pvParameters);
                void rtos_startup_err_callback(void * p_instance, void * p_data);
                void rtos_startup_common_init(void);
extern uint32_t g_fsp_common_thread_count;

                const rm_freertos_port_parameters_t sample_thread_parameters =
                {
                    .p_context = (void *) NULL,
                };

                void sample_thread_create (void)
                {
                    /* Increment count so we will know the number of threads created in the FSP Configuration editor. */
                    g_fsp_common_thread_count++;

                    /* Initialize each kernel object. */
                    

                    #if 1
                    sample_thread = xTaskCreateStatic(
                    #else
                    BaseType_t sample_thread_create_err = xTaskCreate(
                    #endif
                        sample_thread_func,
                        (const char *)"Start Task",
                        0x500/4, // In words, not bytes
                        (void *) &sample_thread_parameters, //pvParameters
                        1,
                        #if 1
                        (StackType_t *)&sample_thread_stack,
                        (StaticTask_t *)&sample_thread_memory
                        #else
                        & sample_thread
                        #endif
                    );

                    #if 1
                    if (NULL == sample_thread)
                    {
                        rtos_startup_err_callback(sample_thread, 0);
                    }
                    #else
                    if (pdPASS != sample_thread_create_err)
                    {
                        rtos_startup_err_callback(sample_thread, 0);
                    }
                    #endif
                }
                static void sample_thread_func (void * pvParameters)
                {
                    /* Initialize common components */
                    rtos_startup_common_init();

                    /* Initialize each module instance. */
                    

                    /* Enter user code for this thread. Pass task handle. */
                    sample_thread_entry(pvParameters);
                }
