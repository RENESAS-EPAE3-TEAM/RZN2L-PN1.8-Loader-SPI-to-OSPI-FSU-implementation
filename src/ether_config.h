
#ifndef ETHER_CONFIG_H_
#define ETHER_CONFIG_H_

#include "hal_data.h"
#include "r_ether_phy_rzt2.h"
#include "r_ether_phy_api_rzt2.h"
#include "r_ether_switch_rzt2.h"
#include "r_ether_switch_api_rzt2.h"
#include "r_ether_rzt2.h"
#include "r_ether_api_rzt2.h"
#include "r_ether_cfg.h"
#include "r_ether_phy_cfg.h"

void ether_buffers_init(void);

extern void evma_ptpout_0_isr(void);
extern void evma_ptpout_1_isr(void);
extern void evma_ptpout_2_isr(void);
extern void evma_ptpout_3_isr(void);

extern void EDDP_LL_ISR_NewCycle(void);
extern void EDDP_LL_ISR_PhaseStartRed(void);
extern void EDDP_LL_ISR_PhaseStartGreen(void);
extern void EDDP_LL_ISR_PhaseStartYellow(void);
extern void EDDP_LL_ISR_PatternAdjust(void);

/* ether_phy on ether_phy Instance. */
extern const ether_phy_instance_t g_ether_phy0;

/* Access the Ethernet PHY instance using these structures when calling API functions directly (::p_api is not used). */
extern ether_phy_instance_ctrl_t g_ether_phy0_ctrl;
extern const ether_phy_cfg_t g_ether_phy0_cfg;
/* ether on ether Instance. */
extern ether_instance_t g_ether0;
extern ether_switch_portentry_t    switch_config[4];
/* Access the Ethernet instance using these structures when calling API functions directly (::p_api is not used). */
extern ether_instance_ctrl_t g_ether0_ctrl;
extern const ether_cfg_t g_ether0_cfg;

#ifndef vEtherISRCallback
void vEtherISRCallback(ether_callback_args_t *p_args);
#endif

/* Minimum Ethernet frame size (without CRC). */
#ifndef MINIMUM_ETHERNET_FRAME_SIZE
#define MINIMUM_ETHERNET_FRAME_SIZE               (60U)
#endif
#ifndef MAXIMUM_ETHERNET_FRAME_SIZE
#define MAXIMUM_ETHERNET_FRAME_SIZE              ETHER_BUFFER_SIZE // (1514U)
#endif
#ifndef ETHER_GMAC_INTERRUPT_FACTOR_RECEPTION
#define ETHER_GMAC_INTERRUPT_FACTOR_RECEPTION    (0x000000C0)
#endif
#ifndef ETHER_LINK_STATUS_CHECK_INTERVAL
#define ETHER_LINK_STATUS_CHECK_INTERVAL          (1000)
#endif
#ifndef UNSIGNED_SHORT_RANDOM_NUMBER_MASK
#define UNSIGNED_SHORT_RANDOM_NUMBER_MASK         (0xFFFFUL)
#endif

#ifdef USE_ETHERNET
extern void handle_error (fsp_err_t err);
#endif

#endif /* ETHER_CONFIG_H_ */
