#include "platform.h"
#include "gpio.h"

#include "netconf.h"
#include "ethernetif.h"

#include "lwip/netif.h"
#include "lwip/stats.h"
#include "lwipopts.h"

#include "bsp_eth.h"
#include "eth.h"
#include "exti.h"
#include "systime.h"
#include "ee_config.h"

#include "console.h"

#define PHY_ADDRESS       0x00

ETH_InitTypeDef ETH_InitStructure;
volatile uint32_t eth_state_change = 0;

static struct console_command_t eth_cmd;

//static void eth_phy_inthandler(void);

/*
const struct gpio_init_table_t eth_clk_gpio[] = {
	{ // MCO
		.gpio = GPIOA,
		.pin = GPIO_Pin_8,
#ifdef STM32F1
		.mode = GPIO_Mode_AF_PP,
#endif
#ifdef STM32F4
		.mode = GPIO_Mode_AF,
		.otype = GPIO_OType_PP,
#endif
		.speed = GPIO_Speed_50MHz,
	},
};
*/

void eth_init()
{
//	gpio_init(eth_clk_gpio, ARRAY_SIZE(eth_clk_gpio)); 

//#ifdef STM32F107RC
//	// Init 50MHz output on MCO pin
//	/* Set PLL3 clock output to 50MHz (25MHz /5 *10 =50MHz) */
//	RCC_PLL3Config(RCC_PLL3Mul_10);
//	/* Enable PLL3 */
//	RCC_PLL3Cmd(ENABLE);
//	/* Wait till PLL3 is ready */
//	while (RCC_GetFlagStatus(RCC_FLAG_PLL3RDY) == RESET);
//	/* Get PLL3 clock on PA8 pin (MCO) */
//	RCC_MCOConfig(RCC_MCO_PLL3CLK);
//#endif
//#ifdef STM32F417VG
//	RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
//#endif

#ifdef STM32F1
	RCC->AHBENR |= RCC_AHBENR_ETHMACEN | RCC_AHBENR_ETHMACTXEN | RCC_AHBENR_ETHMACRXEN;
	AFIO->MAPR |= (1 << 23); 
	if (eth_gpio_remap) {
		AFIO->MAPR |= (1 << 21);
	}
#endif

	systime_delay(1);

	// bring phy into reset and set strapping pins
	gpio_init(eth_init_gpio, eth_init_gpio_cnt);

	systime_delay(1);

	// phy out of reset and pins into "normal" mode
	gpio_init(eth_gpio, eth_gpio_cnt);

	systime_delay(1);

#ifdef STM32F4
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx | RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);
	SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);
#endif

	ETH_DeInit();
	ETH_SoftwareReset();
	// if sw gets stuck here, MAC ref clock is probably missing.
	while (ETH_GetSoftwareResetStatus() == SET);

	/* ETHERNET Configuration ------------------------------------------------------*/
	/* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
	ETH_StructInit(&ETH_InitStructure);

	/* Fill ETH_InitStructure parametrs */
	/*------------------------   MAC   -----------------------------------*/
	if (EE_FLAGS(EE_FLAGS_AUTONEG)) {
		ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
	} else {
		if (EE_FLAGS(EE_FLAGS_100M)) {
			ETH_InitStructure.ETH_Speed = ETH_Speed_100M;
		} else {
			ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
		}

		if (EE_FLAGS(EE_FLAGS_DUPLEX)) {
			ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;
		} else {
			ETH_InitStructure.ETH_Mode = ETH_Mode_HalfDuplex;
		}
	}

	ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
	ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
	ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
	ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
	ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
	ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
	ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
	ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

	/*------------------------   DMA   -----------------------------------*/

	/* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
	the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum,
	if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
	ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
	ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

	ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
	ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
	ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
	ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
	ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
	ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

	/* Configure Ethernet */
	ETH_Init(&ETH_InitStructure, PHY_ADDRESS);

#if STM32_ETH_INTERRUPTS
	/* Enable the Ethernet Rx Interrupt */
	ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R, ENABLE);

	NVIC_SetPriority(ETH_IRQn, 2);
	NVIC_EnableIRQ(ETH_IRQn);
#endif

//	ETH_WritePHYRegister(PHY_ADDRESS, PHY_BCR, 0x8000);
	//ETH_Stop();

#if CONSOLE_DISABLE == 0
	console_add_command(&eth_cmd);
#endif
}

void eth_irq_enable(void)
{
	uint16_t regvalue = ETH_ReadPHYRegister(PHY_ADDRESS, PHY_IMR);
	regvalue |= PHY_INT_LINK;
	ETH_WritePHYRegister(PHY_ADDRESS, PHY_IMR, regvalue);

	//exti_set_handler(EXTI_5, eth_phy_inthandler);
	//exti_enable(EXTI_5, EXTI_Trigger_Falling, GPIO_PortSourceGPIOA);
}

void eth_irq_disable(void)
{
	//exti_disable(EXTI_5);
}

//void ETH_IRQHandler(void)
//{
//	/* Handles all the received frames */
//	while(ETH_GetRxPktSize() != 0) {
//		LwIP_Pkt_Handle();
//	}
//
//	/* Clear the Eth DMA Rx IT pending bits */
//	ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
//	ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
//}

void eth_periodic(void)
{
	static uint32_t last_time = 0;
	static uint16_t last_state = PHY_BSR_LINK;

	ethernetif_input(&netif);

	if (systime_get() - last_time > SYSTIME_SEC(1)) {
		uint16_t regvalue = ETH_ReadPHYRegister(PHY_ADDRESS, PHY_BSR);

		// link status bit changed;
		if ((last_state ^ regvalue) & PHY_BSR_LINK) {
			//console_printf(CON_ERR, "Eth: state changed %04x %d\n", regvalue, regvalue & PHY_BSR_LINK);
			if (regvalue & PHY_BSR_LINK) {
				//netif_set_link_up(&netif);
				//netif_set_link_down(&netif);
				netif_set_link_up(&netif);
			} else {
				netif_set_link_down(&netif);
			}
		}

		last_state = regvalue;
		last_time = systime_get();
	}
}
/*
static void eth_phy_inthandler(void)
{
	eth_state_change = 1;
}
*/

#if CONSOLE_DISABLE == 0
static const struct console_stats_t eth_stats[] = {
	{ offsetof(ETH_TypeDef, MACCR),       0, 0xffffffff, "Mac control register:   0x%08x"},
//	{ offsetof(ETH_TypeDef, MACDBGR),     0, 0xffffffff, "Mac debug register      0x%08x"},
	{ offsetof(ETH_TypeDef, DMASR),       0, 0xffffffff, "Dma status register:    0x%08x"},
	{ offsetof(ETH_TypeDef, DMAOMR),      0, 0xffffffff, "Dma operation mode:     0x%08x"},
	{ offsetof(ETH_TypeDef, MMCTGFSCCR),  0, 0xffffffff, "tx good - 1 collision:  %d"},
	{ offsetof(ETH_TypeDef, MMCTGFMSCCR), 0, 0xffffffff, "tx good - >1 collision: %d"},
	{ offsetof(ETH_TypeDef, MMCTGFCR),    0, 0xffffffff, "tx good:                %d"},
	{ offsetof(ETH_TypeDef, MMCRFCECR),   0, 0xffffffff, "rx w/ CRC error:        %d"},
	{ offsetof(ETH_TypeDef, MMCRFAECR),   0, 0xffffffff, "rx w/ alignment error:  %d"},
	{ offsetof(ETH_TypeDef, MMCRGUFCR),   0, 0xffffffff, "rx good unicast frames: %d"},
	{ offsetof(ETH_TypeDef, DMAMFBOCR),   0, 0x0000ffff, "Missed frames - ctrl:   %d"},
	{ offsetof(ETH_TypeDef, DMAMFBOCR),  17, 0x000007ff, "Missed frames - app:    %d"},
};

static const struct console_stats_t netif_stats[] = {
	{ offsetof(struct netif, ip_addr),   0, 0xffffffff, "Ip addr: %A"},
	{ offsetof(struct netif, netmask),   0, 0xffffffff, "Netmask: %A"},
	{ offsetof(struct netif, gw),        0, 0xffffffff, "Gateway: %A"},
	{ offsetof(struct netif, mtu),       0, 0x0000ffff, "Mtu: %d"},
	{ offsetof(struct netif, flags),     0, 0x000000ff, "Flags: %x"},
};

static const struct console_stats_t dhcp_stats[] = {
	{ offsetof(struct dhcp, state),            0, 0x000000ff, "DHCP state: %d"},
	{ offsetof(struct dhcp, tries),            0, 0x000000ff, "DHCP tries: %d"},
	{ offsetof(struct dhcp, server_ip_addr),   0, 0xffffffff, "DHCP server ip: %A"},
	{ offsetof(struct dhcp, offered_t0_lease), 0, 0xffffffff, "DHCP lease time: %d"},
};

static const struct {
	uint8_t reg;
	char *str;
} phy_stats[] = {
	{ PHY_BCR, "Base control register: %04x\n"},
	{ PHY_BSR, "Base status register:  %04x\n"},
	{       2, "PHY identifier 1:      %04x\n"},
	{       3, "PHY identifier 2:      %04x\n"},
	{       4, "Autoneg advertisment:  %04x\n"},
	{       5, "Autoneg link partner:  %04x\n"},
	{       6, "Autoneg expansion reg: %04x\n"},
	{      17, "Mode control/status:   %04x\n"},
	{      18, "Special Modes:         %04x\n"},
	{      26, "Symbol error counter:  %04x\n"},
	{      27, "Control/status ind:    %04x\n"},
	{      29, "Interrupt source reg:  %04x\n"},
	{      30, "Interrupt mask reg:    %04x\n"},
	{      31, "PHY special control:   %04x\n"},
};

static void cmd_eth_print(struct console_session_t *css, const struct console_stats_t *cs, uint32_t base)
{
	uint32_t val;

	val = *(uint32_t *)((uint32_t)base + cs->offset);
	val = (val >> cs->shift) & cs->mask;

	console_session_printf(css, cs->str, val);
	console_session_printf(css, "\n");
}

#if LWIP_STATS
static const struct {
	struct stats_proto *proto;
	const char *name;
} eth_lwip_proto[] = {
	{ &lwip_stats.tcp, "TCP"},
	{ &lwip_stats.udp, "UDP"},
	{ &lwip_stats.icmp, "ICMP"},
	{ &lwip_stats.ip, "IP"},
	{ &lwip_stats.ip_frag, "IP_FRAG"},
	{ &lwip_stats.etharp, "ETHARP"},
	{ &lwip_stats.link, "LINK"},
};

static void cmd_eth_lwip_proto(struct console_session_t *css, struct stats_proto *proto, const char *name)
{
	console_session_printf(css, "%s\t", name);
	console_session_printf(css, "xmit: %u\t", proto->xmit);
	console_session_printf(css, "recv: %u\t", proto->recv);
	console_session_printf(css, "fw: %u\t", proto->fw);
	console_session_printf(css, "drop: %u\t", proto->drop);
	console_session_printf(css, "chkerr: %u\t", proto->chkerr);
	console_session_printf(css, "lenerr: %u\t", proto->lenerr);
	console_session_printf(css, "memerr: %u\t", proto->memerr);
	console_session_printf(css, "rterr: %u\t", proto->rterr);
	console_session_printf(css, "proterr: %u\t", proto->proterr);
	console_session_printf(css, "opterr: %u\t", proto->opterr);
	console_session_printf(css, "err: %u\t", proto->err);
	console_session_printf(css, "cachehit: %u\n", proto->cachehit);
}

static void cmd_eth_lwip_mem(struct console_session_t *css, struct stats_mem *mem)
{
	console_session_printf(css, "MEM %s\t", mem->name);
	console_session_printf(css, "avail: %"U32_F"\t", (u32_t)mem->avail);
	console_session_printf(css, "used: %"U32_F"\t", (u32_t)mem->used);
	console_session_printf(css, "max: %"U32_F"\t", (u32_t)mem->max);
	console_session_printf(css, "err: %"U32_F"\n", (u32_t)mem->err);
}
#endif

static uint8_t eth_cmd_handler(struct console_session_t *cs, char **args)
{
	uint8_t ret = 1;
	uint32_t i;
	
	if (args[0] == NULL) {
		console_session_printf(cs, "Valid: st, lw, if, phy\n");
		return 1;
	}

	if (strncmp(args[0], "st", strlen(args[0])) == 0) {
		console_session_printf(cs, "Ethernet statistics\n");
		for (i = 0; i < ARRAY_SIZE(eth_stats); i++) {
			cmd_eth_print(cs, &eth_stats[i], (uint32_t)ETH);
		}
		ret = 0;
#if LWIP_STATS
	} else if (strncmp(args[0], "lw", strlen(args[0])) == 0) {
		console_session_printf(cs, "LWIP statistics\n");
		for (i = 0; i < ARRAY_SIZE(eth_lwip_proto); i++) {
			cmd_eth_lwip_proto(cs, eth_lwip_proto[i].proto, eth_lwip_proto[i].name);
		}
		for (i = 0; i < MEMP_MAX; i++) {
			cmd_eth_lwip_mem(cs, lwip_stats.memp[i]);
		}
#if MEM_STATS
		cmd_eth_lwip_mem(cs, lwip_stats.mem);
#endif

#endif
	} else if (strncmp(args[0], "if", strlen(args[0])) == 0) {
		console_session_printf(cs, "Interface statistics\n");
		for (i = 0; i < ARRAY_SIZE(netif_stats); i++) {
			cmd_eth_print(cs, &netif_stats[i], (uint32_t)&netif);
		}
		if (netif_get_client_data(&netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP) != NULL) {
			for (i = 0; i < ARRAY_SIZE(dhcp_stats); i++) {
				cmd_eth_print(cs, &dhcp_stats[i], (uint32_t)netif_get_client_data(&netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP));
			}
		}
		ret = 0;
	} else if (strncmp(args[0], "phy", strlen(args[0])) == 0) {
		console_session_printf(cs, "PHY registers\n");
		for (i = 0; i < ARRAY_SIZE(phy_stats); i++) {
			console_session_printf(cs, phy_stats[i].str, ETH_ReadPHYRegister(PHY_ADDRESS, phy_stats[i].reg));
		}

	} else if (strncmp(args[0], "promisc", strlen(args[0])) == 0) {
		uint32_t reg = ETH->MACFFR & 0x01;

		reg = !reg;
		if (reg) {
			ETH->MACFFR |= 0x01;
		} else {
			ETH->MACFFR &= ~0x01;
		}

		console_session_printf(cs, "Promisc mode: %s\n", reg?"enabled":"disabled");
	} else if (strncmp(args[0], "link", strlen(args[0])) == 0) {
		netif_set_link_down(&netif);

		netif_set_link_up(&netif);
	}
	return ret;
}

static struct console_command_t eth_cmd = {
	"eth",
	eth_cmd_handler,
	"Ethernet menu",
	"Ethernet\n st - print stats\n lw - lwip stack statistics \n if - current interface configuration (addr, netmask, gw)",
	NULL
};
#endif
