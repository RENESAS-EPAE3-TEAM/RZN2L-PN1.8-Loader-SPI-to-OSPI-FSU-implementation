/* generated thread header file - do not edit */
#ifndef APP_PRINTETH_THREAD_H_
#define APP_PRINTETH_THREAD_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void app_printeth_thread_entry(void * pvParameters);
                #else
                extern void app_printeth_thread_entry(void * pvParameters);
                #endif
FSP_HEADER
FSP_FOOTER
#endif /* APP_PRINTETH_THREAD_H_ */
