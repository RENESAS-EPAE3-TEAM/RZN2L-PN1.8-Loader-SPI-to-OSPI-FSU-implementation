/*******************************************************************************
Includes   <System Includes> , "Project Includes"
*******************************************************************************/
#include <stdint.h>
#include "hal_data.h"
#include "sio_char.h"
#include "r_uart_api.h"
#include "r_sci_uart.h"


/*******************************************************************************
Typedef definitions
*******************************************************************************/


/*******************************************************************************
Macro definitions
*******************************************************************************/


/*******************************************************************************
Imported global variables and functions (from other files)
*******************************************************************************/


/*******************************************************************************
Exported global variables and functions (to be accessed by other files)
*******************************************************************************/


/*******************************************************************************
Private global variables and functions
*******************************************************************************/
volatile uint32_t g_transfer_complete = 0;
volatile uint32_t g_receive_complete  = 0;
uint8_t  g_out_of_band_received[TRANSFER_LENGTH];
uint32_t g_out_of_band_index = 0;

/*******************************************************************************
* Function Name: IoGetchar
* Description  : One character is received from SCI, and it's data is returned.
*              : This function keeps waiting until it can obtain the receiving data.
* Arguments    : none
* Return Value : Character to receive (Byte).
*******************************************************************************/
int32_t IoGetchar (void)
{
    int32_t    data;
    uint8_t    data_char;

    if( R_SCI_UART_Read(&g_uart0_ctrl, &data_char, TRANSFER_LENGTH) != FSP_SUCCESS )
    {
        return -1;
    }
    while (!g_receive_complete)
    {
#ifdef PrintfDebug
    	vTaskDelay(1);
#endif
    }
    g_receive_complete = 0;

    data = (int32_t)(data_char &0xff);
    return data;
}

/*******************************************************************************
* Function Name: IoPutchar
* Description  : Character "buffer" is output to SCIF.
*              : This function keeps waiting until it becomes the transmission
*              : enabled state.
* Arguments    : int_t buffer : character to output
* Return Value : None
*******************************************************************************/
void IoPutchar (int32_t buffer)
{
    R_SCI_UART_Write(&g_uart0_ctrl, (uint8_t*) &buffer, TRANSFER_LENGTH);
    while (!g_transfer_complete)
    {
    }
    g_transfer_complete = 0;
}

void user_uart_callback (uart_callback_args_t * p_args)
{
    /* Handle the UART event */
    switch (p_args->event)
    {
        /* Received a character */
        case UART_EVENT_RX_CHAR:
        {
            /* Only put the next character in the receive buffer if there is space for it */
            if (sizeof(g_out_of_band_received) > g_out_of_band_index)
            {
                /* Write either the next one or two bytes depending on the receive data size */
                if (UART_DATA_BITS_8 >= g_uart0_cfg.data_bits)
                {
                    g_out_of_band_received[g_out_of_band_index++] = (uint8_t) p_args->data;
                }
                else
                {
                    uint16_t * p_dest = (uint16_t *) &g_out_of_band_received[g_out_of_band_index];
                    *p_dest              = (uint16_t) p_args->data;
                    g_out_of_band_index += 2;
                }
            }
            break;
        }
        /* Receive complete */
        case UART_EVENT_RX_COMPLETE:
        {
            g_receive_complete = 1;
            break;
        }
        /* Transmit complete */
        case UART_EVENT_TX_COMPLETE:
        {
            g_transfer_complete = 1;
            break;
        }
        default:
        {
        }
    }
}
/* End of File */

