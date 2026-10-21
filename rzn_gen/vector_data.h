/* generated vector header file - do not edit */
        #ifndef VECTOR_DATA_H
        #define VECTOR_DATA_H
        #include "bsp_api.h"
                /* Number of interrupts allocated */
        #ifndef VECTOR_DATA_IRQ_COUNT
        #define VECTOR_DATA_IRQ_COUNT    (21)
        #endif
        /* ISR prototypes */
        void cmtw_cm_int_isr(void);
        void cmtw_ic0_int_isr(void);
        void cmtw_ic1_int_isr(void);
        void cmtw_oc0_int_isr(void);
        void cmtw_oc1_int_isr(void);
        void ether_isr_pmt(void);
        void ether_isr_sbd(void);
        void ether_switch_isr_intr(void);
        void evma_ptpout_0_isr(void);
        void evma_ptpout_1_isr(void);
        void evma_ptpout_2_isr(void);
        void evma_ptpout_3_isr(void);
        void EDDP_LL_ISR_PhaseStartRed(void);
        void EDDP_LL_ISR_PhaseStartGreen(void);
        void EDDP_LL_ISR_PhaseStartYellow(void);
        void EDDP_LL_ISR_PatternAdjust(void);
        void sci_uart_eri_isr(void);
        void sci_uart_rxi_isr(void);
        void sci_uart_txi_isr(void);
        void sci_uart_tei_isr(void);

        /* Vector table allocations */
        #define VECTOR_NUMBER_CMTW0_CMWI ((IRQn_Type) 59) /* CMTW0_CMWI (CMTW0 Compare match) */
        #define VECTOR_NUMBER_CMTW0_IC0I ((IRQn_Type) 60) /* CMTW0_IC0I (CMTW0 Input capture of register 0) */
        #define VECTOR_NUMBER_CMTW0_IC1I ((IRQn_Type) 61) /* CMTW0_IC1I (CMTW0 Input capture of register 1) */
        #define VECTOR_NUMBER_CMTW0_OC0I ((IRQn_Type) 62) /* CMTW0_OC0I (CMTW0 Output compare of register 0) */
        #define VECTOR_NUMBER_CMTW0_OC1I ((IRQn_Type) 63) /* CMTW0_OC1I (CMTW0 Output compare of register 1) */
        #define VECTOR_NUMBER_CMTW1_CMWI ((IRQn_Type) 64) /* CMTW1_CMWI (CMTW1 Compare match) */
        #define VECTOR_NUMBER_GMAC_PMT ((IRQn_Type) 251) /* GMAC_PMT (GMAC1 power management) */
        #define VECTOR_NUMBER_GMAC_SBD ((IRQn_Type) 252) /* GMAC_SBD (GMAC1 general interrupt) */
        #define VECTOR_NUMBER_ETHSW_INTR ((IRQn_Type) 253) /* ETHSW_INTR (Ethernet Switch interrupt) */
        #define VECTOR_NUMBER_ETHSW_PTPOUT0 ((IRQn_Type) 269) /* ETHSW_PTPOUT0 (Ethernet switch timer pulse output 0) */
        #define VECTOR_NUMBER_ETHSW_PTPOUT1 ((IRQn_Type) 270) /* ETHSW_PTPOUT1 (Ethernet switch timer pulse output 1) */
        #define VECTOR_NUMBER_ETHSW_PTPOUT2 ((IRQn_Type) 271) /* ETHSW_PTPOUT2 (Ethernet switch timer pulse output 2) */
        #define VECTOR_NUMBER_ETHSW_PTPOUT3 ((IRQn_Type) 272) /* ETHSW_PTPOUT3 (Ethernet switch timer pulse output 3) */
        #define VECTOR_NUMBER_ETHSW_TDMAOUT0 ((IRQn_Type) 273) /* ETHSW_TDMAOUT0 (Ethernet Switch TDMA timer output 0) */
        #define VECTOR_NUMBER_ETHSW_TDMAOUT1 ((IRQn_Type) 274) /* ETHSW_TDMAOUT1 (Ethernet Switch TDMA timer output 1) */
        #define VECTOR_NUMBER_ETHSW_TDMAOUT2 ((IRQn_Type) 275) /* ETHSW_TDMAOUT2 (Ethernet Switch TDMA timer output 2) */
        #define VECTOR_NUMBER_ETHSW_TDMAOUT3 ((IRQn_Type) 276) /* ETHSW_TDMAOUT3 (Ethernet Switch TDMA timer output 3) */
        #define VECTOR_NUMBER_SCI0_ERI ((IRQn_Type) 288) /* SCI0_ERI (SCI0 Receive error) */
        #define VECTOR_NUMBER_SCI0_RXI ((IRQn_Type) 289) /* SCI0_RXI (SCI0 Receive data full) */
        #define VECTOR_NUMBER_SCI0_TXI ((IRQn_Type) 290) /* SCI0_TXI (SCI0 Transmit data empty) */
        #define VECTOR_NUMBER_SCI0_TEI ((IRQn_Type) 291) /* SCI0_TEI (SCI0 Transmit end) */
        typedef enum IRQn {
            SoftwareGeneratedInt0 = -32,
            SoftwareGeneratedInt1 = -31,
            SoftwareGeneratedInt2 = -30,
            SoftwareGeneratedInt3 = -29,
            SoftwareGeneratedInt4 = -28,
            SoftwareGeneratedInt5 = -27,
            SoftwareGeneratedInt6 = -26,
            SoftwareGeneratedInt7 = -25,
            SoftwareGeneratedInt8 = -24,
            SoftwareGeneratedInt9 = -23,
            SoftwareGeneratedInt10 = -22,
            SoftwareGeneratedInt11 = -21,
            SoftwareGeneratedInt12 = -20,
            SoftwareGeneratedInt13 = -19,
            SoftwareGeneratedInt14 = -18,
            SoftwareGeneratedInt15 = -17,
            DebugCommunicationsChannelInt = -10,
            PerformanceMonitorCounterOverflowInt = -9,
            CrossTriggerInterfaceInt = -8,
            VritualCPUInterfaceMaintenanceInt = -7,
            HypervisorTimerInt = -6,
            VirtualTimerInt = -5,
            NonSecurePhysicalTimerInt = -2,
            CMTW0_CMWI_IRQn = 59, /* CMTW0_CMWI (CMTW0 Compare match) */
            CMTW0_IC0I_IRQn = 60, /* CMTW0_IC0I (CMTW0 Input capture of register 0) */
            CMTW0_IC1I_IRQn = 61, /* CMTW0_IC1I (CMTW0 Input capture of register 1) */
            CMTW0_OC0I_IRQn = 62, /* CMTW0_OC0I (CMTW0 Output compare of register 0) */
            CMTW0_OC1I_IRQn = 63, /* CMTW0_OC1I (CMTW0 Output compare of register 1) */
            CMTW1_CMWI_IRQn = 64, /* CMTW1_CMWI (CMTW1 Compare match) */
            GMAC_PMT_IRQn = 251, /* GMAC_PMT (GMAC1 power management) */
            GMAC_SBD_IRQn = 252, /* GMAC_SBD (GMAC1 general interrupt) */
            ETHSW_INTR_IRQn = 253, /* ETHSW_INTR (Ethernet Switch interrupt) */
            ETHSW_PTPOUT0_IRQn = 269, /* ETHSW_PTPOUT0 (Ethernet switch timer pulse output 0) */
            ETHSW_PTPOUT1_IRQn = 270, /* ETHSW_PTPOUT1 (Ethernet switch timer pulse output 1) */
            ETHSW_PTPOUT2_IRQn = 271, /* ETHSW_PTPOUT2 (Ethernet switch timer pulse output 2) */
            ETHSW_PTPOUT3_IRQn = 272, /* ETHSW_PTPOUT3 (Ethernet switch timer pulse output 3) */
            ETHSW_TDMAOUT0_IRQn = 273, /* ETHSW_TDMAOUT0 (Ethernet Switch TDMA timer output 0) */
            ETHSW_TDMAOUT1_IRQn = 274, /* ETHSW_TDMAOUT1 (Ethernet Switch TDMA timer output 1) */
            ETHSW_TDMAOUT2_IRQn = 275, /* ETHSW_TDMAOUT2 (Ethernet Switch TDMA timer output 2) */
            ETHSW_TDMAOUT3_IRQn = 276, /* ETHSW_TDMAOUT3 (Ethernet Switch TDMA timer output 3) */
            SCI0_ERI_IRQn = 288, /* SCI0_ERI (SCI0 Receive error) */
            SCI0_RXI_IRQn = 289, /* SCI0_RXI (SCI0 Receive data full) */
            SCI0_TXI_IRQn = 290, /* SCI0_TXI (SCI0 Transmit data empty) */
            SCI0_TEI_IRQn = 291, /* SCI0_TEI (SCI0 Transmit end) */
            SHARED_PERIPHERAL_INTERRUPTS_MAX_ENTRIES = BSP_VECTOR_TABLE_MAX_ENTRIES
        } IRQn_Type;
        #endif /* VECTOR_DATA_H */