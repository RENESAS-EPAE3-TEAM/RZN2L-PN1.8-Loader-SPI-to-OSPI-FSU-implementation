/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] =
        {
                        [59] = cmtw_cm_int_isr, /* CMTW0_CMWI (CMTW0 Compare match) */
            [60] = cmtw_ic0_int_isr, /* CMTW0_IC0I (CMTW0 Input capture of register 0) */
            [61] = cmtw_ic1_int_isr, /* CMTW0_IC1I (CMTW0 Input capture of register 1) */
            [62] = cmtw_oc0_int_isr, /* CMTW0_OC0I (CMTW0 Output compare of register 0) */
            [63] = cmtw_oc1_int_isr, /* CMTW0_OC1I (CMTW0 Output compare of register 1) */
            [64] = cmtw_cm_int_isr, /* CMTW1_CMWI (CMTW1 Compare match) */
            [251] = ether_isr_pmt, /* GMAC_PMT (GMAC1 power management) */
            [252] = ether_isr_sbd, /* GMAC_SBD (GMAC1 general interrupt) */
            [253] = ether_switch_isr_intr, /* ETHSW_INTR (Ethernet Switch interrupt) */
            [269] = evma_ptpout_0_isr, /* ETHSW_PTPOUT0 (Ethernet switch timer pulse output 0) */
            [270] = evma_ptpout_1_isr, /* ETHSW_PTPOUT1 (Ethernet switch timer pulse output 1) */
            [271] = evma_ptpout_2_isr, /* ETHSW_PTPOUT2 (Ethernet switch timer pulse output 2) */
            [272] = evma_ptpout_3_isr, /* ETHSW_PTPOUT3 (Ethernet switch timer pulse output 3) */
            [273] = EDDP_LL_ISR_PhaseStartRed, /* ETHSW_TDMAOUT0 (Ethernet Switch TDMA timer output 0) */
            [274] = EDDP_LL_ISR_PhaseStartGreen, /* ETHSW_TDMAOUT1 (Ethernet Switch TDMA timer output 1) */
            [275] = EDDP_LL_ISR_PhaseStartYellow, /* ETHSW_TDMAOUT2 (Ethernet Switch TDMA timer output 2) */
            [276] = EDDP_LL_ISR_PatternAdjust, /* ETHSW_TDMAOUT3 (Ethernet Switch TDMA timer output 3) */
            [288] = sci_uart_eri_isr, /* SCI0_ERI (SCI0 Receive error) */
            [289] = sci_uart_rxi_isr, /* SCI0_RXI (SCI0 Receive data full) */
            [290] = sci_uart_txi_isr, /* SCI0_TXI (SCI0 Transmit data empty) */
            [291] = sci_uart_tei_isr, /* SCI0_TEI (SCI0 Transmit end) */
        };
        #endif