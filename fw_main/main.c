#include "platform.h"

#include <string.h>

#include "rtc.h"
#include "sntp.h"
#include "eth.h"
#include "ethernetif.h"
#include "netconf.h"
#include "systime.h"
#include "ee_config.h"
#include "disc.h"
#include "console.h"
#include "console_telnet.h"
#include "console_uart.h"
#include "ver.h"
#include "random.h"
#include "delay.h"
#include "wdg.h"
#include "led.h"
#include "syscall.h"
#include "can_tcp.h"
#include "mqtt_relay.h"

#ifdef VECT_TAB_RAM
extern uint32_t _isr_vectorsram_offs;
#else
extern uint32_t _isr_vectorsflash_offs;
#endif

void NVIC_Configuration(void)
{
#ifdef VECT_TAB_RAM
	// Set the Vector Table base location at 0x20000000+_isr_vectorsram_offs
	NVIC_SetVectorTable(NVIC_VectTab_RAM, (uint32_t)&_isr_vectorsram_offs);
#else
	// Set the Vector Table base location at 0x08000000+_isr_vectorsflash_offs
	SCB->VTOR = (uint32_t)&_isr_vectorsflash_offs;
#endif /* VECT_TAB_RAM */

	/* 2 bit for pre-emption priority, 2 bits for subpriority */
	SCB->AIRCR = SCB_AIRCR_VECTKEY_Msk | 0x500;
}

int main(void)
{
	SystemInit();

#ifdef STM32F1
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN |RCC_APB2ENR_IOPEEN | RCC_APB2ENR_AFIOEN;
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;

	AFIO->MAPR |= (0x02 << 24); // only swj
#endif
#ifdef STM32F417VG
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE, ENABLE);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_BKPSRAM, ENABLE);
#endif

	PWR->CR |= PWR_CR_DBP;

	NVIC_Configuration();

	delay_init();
	systime_init();

	console_init();
	ee_config_init();
	console_uart_apply();
	ver_init();

	// use mac as seed
	random_init(ee_config.mac[2] << 24 | ee_config.mac[3] << 16 | ee_config.mac[3] << 8 | ee_config.mac[4]);

	if (!EE_FLAGS(EE_FLAGS_WDG_DISABLE)) {
//		wdg_enable();
	}

	LwIP_Init();

	console_printf(CON_ERR, "We are online\n\r");
	console_telnet_init();

	led_init();

	disc_init();
  can_tcp_init();

	rtc_init();
	if (ee_config.sntp_server.active) {
		sntp_init();
	}
//	syscall_init();
	mqtt_relay_init();

	led_set(0, LED_3BLINK);

	while (1) {
		eth_periodic();
		systime_periodic();

		led_periodic();

		wdg_feed();
		ee_config_periodic();
		console_periodic();
		can_tcp_periodic();
		mqtt_relay_periodic();
		__WFI();
	}
}

void ee_config_app_restart(void)
{
	console_uart_apply();
}

void netif_ext_status_callback(struct netif* nif, netif_nsc_reason_t reason, const netif_ext_callback_args_t* args)
{
	(void) args;
	(void) nif;

	if (reason & (LWIP_NSC_IPV4_ADDRESS_CHANGED | LWIP_NSC_IPV4_GATEWAY_CHANGED | LWIP_NSC_IPV4_NETMASK_CHANGED | LWIP_NSC_IPV4_SETTINGS_CHANGED)) {
//		console_telnet_restart();
	}
}
