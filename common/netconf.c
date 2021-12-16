
#include "platform.h"

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_frag.h"
#include "lwip/priv/tcp_priv.h"

#include "ethernetif.h"
#include "disc.h"
#include "netconf.h"
#include <stdio.h>

#include "console.h"
#include "systime.h"
#include "eth.h"

#include "ee_config.h"

struct netif netif;
netif_ext_callback_t netif_callback;
extern void netif_ext_status_callback(struct netif* netif, netif_nsc_reason_t reason, const netif_ext_callback_args_t* args);
/**
  * @brief  Initializes the lwIP stack
  * @param  None
  * @retval None
  */
void LwIP_Init(void)
{
	// initialize Lwip related stuff
	mem_init();

	memp_init();

#if LWIP_IPV4
#if IP_REASSEMBLY
	systime_add(ip_reass_tmr, IP_TMR_INTERVAL);
#endif
#if LWIP_ARP
	systime_add(etharp_tmr, ARP_TMR_INTERVAL);
#endif
#if LWIP_ACD
	systime_add(acd_tmr, ACD_TMR_INTERVAL);
#endif /* LWIP_ACD */
#if LWIP_AUTOIP
	systime_add(autoip_tmr, AUTOIP_TMR_INTERVAL);
#endif
#if LWIP_IGMP
	systime_add(igmp_timer, IGMP_TMR_INTERVAL);
#endif
#if LWIP_DNS
	systime_add(dns_timer, DNS_TMR_INTERVAL);
#endif

	systime_add(tcp_tmr, TCP_TMR_INTERVAL);
	systime_add(etharp_tmr, ARP_TMR_INTERVAL);
#endif

#if LWIP_DNS
	systime_add(dns_tmr, DNS_TMR_INTERVAL);
#endif

#if LWIP_IPV6
	systime_add(nd6_tmr, ND6_TMR_INTERVAL);
#if LWIP_IPV6_REASS
	systime_add(ip6_reass_tmr, IP6_REASS_TMR_INTERVAL);
#endif
#if LWIP_IPV6_MLD
	systime_add(mld6_tmr, MLD6_TMR_INTERVAL);
#endif
#if LWIP_IPV6_DHCP6
	systime_add(dhcp6_tmr, DHCP6_TIMER_MSECS);
#endif
#endif

#if LWIP_DHCP
	if (EE_FLAGS(EE_FLAGS_DHCP)) {
		ee_config.addr.addr = 0;
		ee_config.netmask.addr = 0;
		ee_config.gw.addr = 0;

		systime_add(dhcp_fine_tmr, DHCP_FINE_TIMER_MSECS);
		systime_add(dhcp_coarse_tmr, DHCP_COARSE_TIMER_MSECS);
	}
#endif

	// now initialize interface
	eth_init();

	ethernetif_setmac(ee_config.mac);

	netif_add(&netif, &ee_config.addr, &ee_config.netmask, &ee_config.gw, (void *)&ETH_InitStructure, &ethernetif_init, &ethernet_input);

	netif_set_default(&netif);
	//netif_set_link_down(&netif);
	netif_set_link_callback(&netif, ethernetif_update_config);
	netif_add_ext_callback(&netif_callback, netif_ext_status_callback);

	/*  When the netif is fully configured this function must be called.*/
	netif_set_up(&netif);
}

void netconf_start_dhcp(void)
{
	console_printf(CON_WARN, "Eth: Starting DHCP\n");
	dhcp_start(&netif);
}

void ethernetif_notify_conn_changed(struct netif *nif)
{
	if (netif_is_link_up(nif)) {
		uint16_t regvalue = ETH_ReadPHYRegister(0, PHY_BCR);
		if (regvalue & PHY_BCR_AUTONEG) {
			regvalue = ETH_ReadPHYRegister(0, PHY_SR);
			console_printf(CON_WARN, "Eth: link up! %s %s\n", (regvalue & PHY_SPEED_STATUS)?"10":"100", (regvalue & PHY_DUPLEX_STATUS)?"Full":"Half");
		} else {
			console_printf(CON_WARN, "Eth: link up!\n");
		}

		// restart dhcp process
		if (EE_FLAGS(EE_FLAGS_DHCP)) {
			dhcp_start(nif);
			//systime_add_oneshot(netconf_start_dhcp, 500);
		} else {
			disc_send_gw();
		}
	} else {
		console_printf(CON_WARN, "Eth: link down!\n");
	}
}

