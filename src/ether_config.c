#include "ether_config.h"

/* ether phy control block              */
ether_phy_instance_ctrl_t   phy_ctrl[3] =
{
    {.open = 0U, .p_ether_phy_cfg = NULL, .p_reg_etherc = NULL, .local_advertise = 0U},
    {.open = 0U, .p_ether_phy_cfg = NULL, .p_reg_etherc = NULL, .local_advertise = 0U},
    {.open = 0U, .p_ether_phy_cfg = NULL, .p_reg_etherc = NULL, .local_advertise = 0U}
};
/*
 *   PHY driver's data
 *
 * [RZ/N1S]
 * - converter1:          GMAC1         PHY-0x08
 * - converter2: switch3: GMAC2         PHY-0x01
 * o converter3: switch2: GMAC2(port2): PHY-0x0A
 * o converter4: switch1: GMAC2(port1): PHY-0x04
 * o converter5: switch0: GMAC2(port0): PHY-0x05
 */

#ifdef BSP_PLAT_RZN1SDB /* RZ/N1S */
    #define PHY_ADDR_PORT0  0x05
    #define PHY_ADDR_PORT0  0x04
    #define PHY_ADDR_PORT0  0x0A
#else /* !BSP_PLAT_RZN1SDB */
  #ifdef RZT2_RSK
    #ifdef UPD60620_PHY
      #define PHY_ADDR_PORT0    0
      #define PHY_ADDR_PORT1    1
      #define PHY_ADDR_PORT2   16
    #else
    #define PHY_ADDR_PORT0  0x00      // Axel Changed to support automated PHY detection
    #define PHY_ADDR_PORT1  0x01
    #define PHY_ADDR_PORT2  0x02
    #endif
  #else /* !RZT2_RSK */ /* RZ/T2-FPGA */
    #define PHY_ADDR_PORT0  0x00
    #define PHY_ADDR_PORT1  0x00
    #define PHY_ADDR_PORT2  0x00
  #endif /* !RZT2_RSK */ /* RZ/T2-FPGA */
#endif /* !BSP_PLAT_RZN1SDB */

/* configuration of ether phy module */
ether_phy_cfg_t         phy_cfg[3] =
{
    {   /* Port0 */
        .channel                    = 0,                                ///< Channel
        .phy_lsi_address            = PHY_ADDR_PORT0,                   ///< Address of PHY-LSI

        .phy_reset_wait_time        = 10000,    //Loop count?           ///< Wait time for PHY-LSI reboot
        .mii_bit_access_wait_time   = 0,        //unused                ///< Wait time for MII/RMII access

        .flow_control               = ETHER_PHY_FLOW_CONTROL_DISABLE,   ///< Flow control functionally enable or disable
        .mii_type                   = ETHER_PHY_MII_TYPE_MII,           ///< Interface type is MII or RMII

        /** Placeholder for user data.  Passed to the user callback in ether_phy_callback_args_t. */
        .p_context                  = (void const *)0x11,
        .p_extend                   = (void const *)0x22                ///< Placeholder for user extension.
    },
    {   /* Port1 */
        .channel                    = 1,                                ///< Channel
        .phy_lsi_address            = PHY_ADDR_PORT1,                   ///< Address of PHY-LSI

        .phy_reset_wait_time        = 10000,    //Loop count?           ///< Wait time for PHY-LSI reboot
        .mii_bit_access_wait_time   = 0,        //unused                ///< Wait time for MII/RMII access

        .flow_control               = ETHER_PHY_FLOW_CONTROL_DISABLE,   ///< Flow control functionally enable or disable
        .mii_type                   = ETHER_PHY_MII_TYPE_MII,           ///< Interface type is MII or RMII

        /** Placeholder for user data.  Passed to the user callback in ether_phy_callback_args_t. */
        .p_context                  = (void const *)0x11,
        .p_extend                   = (void const *)0x22                ///< Placeholder for user extension.
    },
    {   /* Port2 */
        .channel                    = 2,                                ///< Channel
        .phy_lsi_address            = PHY_ADDR_PORT2,                   ///< Address of PHY-LSI

        .phy_reset_wait_time        = 10000,    //Loop count?           ///< Wait time for PHY-LSI reboot
        .mii_bit_access_wait_time   = 0,        //unused                ///< Wait time for MII/RMII access

        .flow_control               = ETHER_PHY_FLOW_CONTROL_DISABLE,   ///< Flow control functionally enable or disable
        .mii_type                   = ETHER_PHY_MII_TYPE_MII,           ///< Interface type is MII or RMII

        /** Placeholder for user data.  Passed to the user callback in ether_phy_callback_args_t. */
        .p_context                  = (void const *)0x11,
        .p_extend                   = (void const *)0x22                ///< Placeholder for user extension.
    }
};

/* ether phy instance                   */
ether_phy_instance_t    phy_instance[3] =
{
    {
        .p_ctrl             = &phy_ctrl[0],                             ///< Pointer to the control structure for this instance
        .p_cfg              = &phy_cfg[0],                              ///< Pointer to the configuration structure for this instance
        .p_api              = &g_ether_phy_on_ether_phy                 ///< Pointer to the API structure for this instance
    },
    {
        .p_ctrl             = &phy_ctrl[1],                             ///< Pointer to the control structure for this instance
        .p_cfg              = &phy_cfg[1],                              ///< Pointer to the configuration structure for this instance
        .p_api              = &g_ether_phy_on_ether_phy                 ///< Pointer to the API structure for this instance
    },
    {
        .p_ctrl             = &phy_ctrl[2],                             ///< Pointer to the control structure for this instance
        .p_cfg              = &phy_cfg[2],                              ///< Pointer to the configuration structure for this instance
        .p_api              = &g_ether_phy_on_ether_phy                 ///< Pointer to the API structure for this instance
    }
};
extern void vEtherSwitchISRCallback(ether_switch_callback_args_t * p_args);
/* ether switch control block           */

ether_switch_instance_ctrl_t    switch_ctrl = {0};

/* configuration of ether switch module */
ether_switch_cfg_t      switch_cfg =
{
    .channel            = 0,                                            ///< Channel

//    .irq                = IRQ_ETHSW_INTR,                               ///< interrupt number
    .irq                = ETHSW_INTR_IRQn,                               ///< interrupt number
    .interrupt_priority = DEFAULT_INT_PRIORITY,                         ///< interrupt priority
#ifdef RZT2_RSK_PNS
    .p_callback         = vEtherSwitchISRCallback,                                            ///< Callback provided when an ISR occurs.
#else
    .p_callback         = NULL,
#endif
    /** Placeholder for user data.  Passed to the user callback in ether_callback_args_t. */
    .p_context          = 0,                                            ///< Placeholder for user data.
    .p_extend           = 0                                             ///< Placeholder for user extension.
};

/* ether switch instance                */
ether_switch_instance_t switch_instance =
{
    .p_ctrl             = &switch_ctrl,                                 ///< Pointer to the control structure for this instance
    .p_cfg              = &switch_cfg,                                  ///< Pointer to the configuration structure for this instance
    .p_api              = &g_ether_switch_on_ether                      ///< Pointer to the API structure for this instance
};

/* ether control block              */

ether_instance_ctrl_t   ether_ctrl = {0};

uint8_t own_MAC_addr[6]         = { 0x74, 0x90, 0x50, 0x10, 0xE9, 0x07 };
#if __GNUC__
ether_instance_descriptor_t     rx_descriptors[NUM_RX_DESCRIPTOR] BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);
ether_instance_descriptor_t     tx_descriptors[NUM_TX_DESCRIPTOR] BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);
uint8_t                         ether_buffers[NUM_RX_DESCRIPTOR+NUM_TX_DESCRIPTOR][ETHER_BUFFER_SIZE] BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);
uint8_t *                       p_ether_buffers[NUM_RX_DESCRIPTOR+NUM_TX_DESCRIPTOR] BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);

ether_instance_descriptor_t tx_descriptor_red[5] BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);
ether_instance_descriptor_t tx_descriptor_base BSP_PLACE_IN_SECTION(".noncache_buffer.eth") BSP_ALIGN_VARIABLE(4);
#else
//#pragma pack(4)
//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
ether_instance_descriptor_t     rx_descriptors[NUM_RX_DESCRIPTOR];
//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
ether_instance_descriptor_t     tx_descriptors[NUM_TX_DESCRIPTOR];
//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
uint8_t                         ether_buffers[NUM_RX_DESCRIPTOR+NUM_TX_DESCRIPTOR][ETHER_BUFFER_SIZE];
//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
uint8_t *                       p_ether_buffers[NUM_RX_DESCRIPTOR+NUM_TX_DESCRIPTOR];

//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
ether_instance_descriptor_t tx_descriptor_red[5];


//#pragma location="nocache"
#pragma location=".noncache_buffer.eth"
#pragma data_alignment=4
ether_instance_descriptor_t tx_descriptor_base;
#endif

//#pragma pack()
//void * p_profinet_red_frames[5];        // Max 5 pointers to the profinet red frames

/* configuration of ether module        */
const ether_cfg_t             g_ether0_cfg =
{
    .channel            = 0,                                            ///< Channel

    .zerocopy           = ETHER_ZEROCOPY_DISABLE,                       ///< Zero copy enable or disable in Read/Write function
//Axel    .multicast          = ETHER_MULTICAST_DISABLE,                      ///< Multicast enable or disable
    .multicast          = ETHER_MULTICAST_ENABLE,                      ///< Multicast enable or disable
//Axel    .promiscuous        = ETHER_PROMISCUOUS_DISABLE,                    ///< Promiscuous mode enable or disable
    .promiscuous        = ETHER_PROMISCUOUS_ENABLE,                    ///< Promiscuous mode enable or disable
    .flow_control       = ETHER_FLOW_CONTROL_DISABLE,                   ///< Flow control functionally enable or disable
    .broadcast_filter   = 0,                                            ///< Limit of the number of broadcast frames received continuously
    .p_mac_address      = own_MAC_addr,                                 ///< Pointer of MAC address

    .p_rx_descriptors   = rx_descriptors,                               ///< Receive descriptor buffer pool (size is ether_instance_descriptor_t * num_rx_descriptors)
    .p_tx_descriptors   = tx_descriptors,                               ///< Transmit descriptor buffer pool (size is ether_instance_descriptor_t * num_tx_descriptorsr)
 //   .p_tx_descriptor_top = tx_descriptors + NUM_TX_DESCRIPTOR - 1,      ///< Top descriptor in chain

    .num_tx_descriptors = NUM_TX_DESCRIPTOR,                            ///< Number of transmit descriptor
    .num_rx_descriptors = NUM_RX_DESCRIPTOR,                            ///< Number of receive descriptor

    .pp_ether_buffers   = p_ether_buffers,                              ///< Transmit and receive buffer

    .ether_buffer_size  = ETHER_BUFFER_SIZE,                            ///< Size of transmit and receive buffer

//    .sbd_irq                = IRQ_GMAC_SBD,                             ///< BSD interrupt number
    .sbd_irq                = GMAC_SBD_IRQn,                             ///< BSD interrupt number
    .sbd_interrupt_priority = DEFAULT_INT_PRIORITY,                     ///< BSD interrupt priority

//    .pmt_irq                = IRQ_GMAC_PMT,                             ///< BSD interrupt number
    .pmt_irq                = GMAC_PMT_IRQn,                             ///< BSD interrupt number
    .pmt_interrupt_priority = DEFAULT_INT_PRIORITY-1,                   ///< BSD interrupt priority

    .p_callback         = vEtherISRCallback,                            ///< Callback provided when an ISR occurs.

    .p_ether_phy_instance    = &phy_instance[0],                        ///< Pointer to ETHER_PHY instance
    .p_ether_switch_instance = &switch_instance,                        ///< Pointer to SWITCH instance
//    .p_profinet_red_frames =  &p_profinet_red_frames[0],                // pointers to profinet red frames

    /** Placeholder for user data.  Passed to the user callback in ether_callback_args_t. */
    .p_context          = 0,                                            ///< Placeholder for user data.
    .p_extend           = 0                                             ///< Placeholder for user extension.
};

ether_instance_t g_ether0 =
{ .p_ctrl = &ether_ctrl, .p_cfg = &g_ether0_cfg, .p_api = &g_ether_on_ether };
ether_instance_t *gp_freertos_ether = &g_ether0;

ether_switch_portentry_t    switch_config[4] =
{
    { /* Port0 */
        .latency_egress  = 0x0001,
        .latency_ingress = 0x0002,
    },
    {  /* Port1 */
        .latency_egress  = 0x1001,
        .latency_ingress = 0x1002,
    },
    {  /* Port2 */
        .latency_egress  = 0x2001,
        .latency_ingress = 0x2002,
    },
    {  /* Port3 */
        .latency_egress  = 0x3001,
        .latency_ingress = 0x3002,
    }
};

void ether_buffers_init(void)
{
#ifdef USE_RTOS
    for(int32_t index=0; index<(NUM_RX_DESCRIPTOR+NUM_TX_DESCRIPTOR); index++)
    {
        p_ether_buffers[index] = ether_buffers[index];
    }
#endif /* USE_RTOS */
}
