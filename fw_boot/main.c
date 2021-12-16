#include "platform.h"

#include "ethernetif.h"
#include "netconf.h"
#include "httpserver.h" 
#include "wdg.h"

#if DEBUG
#include "console.h"
#endif

#include "led.h"
#include "iap.h"

#include "gpio.h"
#include "disc.h"
#include "systime.h"
#include "eth.h"
#include "ee_config.h"

#include "wdg.h"

#define DEBUG 0

typedef  void (*pFunction)(void);

const struct gpio_init_table_t bld_gpio[] = {
	{
		.gpio = BUTTON_GPIO,
		.pin = BUTTON_PIN,
#ifdef STM32F1
		.mode = BUTTON_PULL,
#endif
#ifdef STM32F4
		.mode = GPIO_Mode_IN,
		.pupd = BUTTON_PULL,
#endif
		.speed = GPIO_SPEED_HIGH,
	},
};


extern uint32_t _isr_vectorsflash_offs;


/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void) __attribute__ ((noreturn));
void main(void)
{
#if DEBUG
	uint8_t reason = 0;
#endif

	// disable SWD and init gpio for button
#ifdef STM32F1
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN |RCC_APB2ENR_IOPEEN | RCC_APB2ENR_AFIOEN;
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;

	AFIO->MAPR |= (0x02 << 24); // only swj
#endif
#ifdef STM32F4
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFG;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOA|RCC_AHB1ENR_GPIOB|RCC_AHB1ENR_GPIOC|RCC_AHB1ENR_GPIOD|RCC_AHB1ENR_GPIOE | RCC_EARLY_AHB1;
//	GPIOA->MODER = 0;
#endif

#if HAVE_BUTTON
	gpio_init(bld_gpio, ARRAY_SIZE(bld_gpio));
#endif

#if HAVE_MAGIC
#ifdef STM32F1
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
#endif
#ifdef STM32F4
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
#endif
	PWR->CR |= PWR_CR_DBP;

	uint32_t mgc;
	uint32_t stak;

#ifdef STM32F1
	mgc = (BKP->DR1 << 16) | BKP->DR2;
	BKP->DR1 = 0;
	BKP->DR2 = 0;
#endif
#ifdef STM32F4
	mgc = RTC->BKP0R;
	RTC->BKP0R = 0;
#endif

	SystemInit();

	SCB->VTOR = (uint32_t)&_isr_vectorsflash_offs;

#if DEBUG
	console_init();
#endif
	ee_config_init();
#if DEBUG
	console_uart_apply();
#endif

#if HAVE_SPIFFS
	{
		// check filesystem for bootloader file.
		uint16_t ret = iap_check_filesystem();
		RTC->BKP1R = 0xDEAD0000 | ret;
	}
#endif

	if (mgc != MAGIC_KEY) {
#if DEBUG
		reason = 1;
#endif
#endif
#if HAVE_BUTTON
		if ((BUTTON_GPIO->IDR & BUTTON_PIN) != BUTTON_EXPECT)  {
#if DEBUG
			reason = 2;
#endif
#endif
			/* check CRC of loaded firmware */
			if (iap_check_fw()) {
#if DEBUG
				reason = 3;
#endif
				/* Check if valid stack address (RAM address) then jump to user application */
				stak = *(__IO uint32_t*)USER_FLASH_FIRST_PAGE_ADDRESS;
#ifdef STM32F1
				if ((stak & 0x2FFE0000 ) == 0x20000000) {
#endif
#ifdef STM32F4
				// stack can be in normal ram, or ccmram
				if (((stak & 0x2FFC0000 ) == 0x20000000) || ((stak & 0x1FFE0000) == 0x10000000)) {
#endif
					/* Jump to user application */
					uint32_t JumpAddress = *(__IO uint32_t*) (USER_FLASH_FIRST_PAGE_ADDRESS + 4);
					pFunction Jump_To_Application = (pFunction) JumpAddress;
					/* Initialize user application's Stack Pointer */
					__set_MSP(*(__IO uint32_t*) USER_FLASH_FIRST_PAGE_ADDRESS);
					Jump_To_Application();
				}
			}
#if HAVE_BUTTON
		}
#endif
#if HAVE_MAGIC
	}
#endif

	systime_init();

	led_init();
	led_set(LED_1, LED_BLINK_FAST | LED_REPEAT);

	/* Initilaize the LwIP stack */
	LwIP_Init();
#if DEBUG
#if CONSOLE_HAVE_TELNET
	console_telnet_init();
#endif
#endif
	disc_init();

#ifdef USE_IAP_HTTP
	/* Initilaize the webserver module */
	IAP_httpd_init();
#endif

#ifdef USE_IAP_TFTP    
	/* Initialize the TFTP server */
	IAP_tftpd_init();
#endif    

#if DEBUG
	console_printf(CON_ERR, "Bootloader started, reason = %d bkp-sram magic = %08x stack = %08x\r\n", reason, mgc, stak);
	iap_init_console();
#endif

	if (!EE_FLAGS(EE_FLAGS_WDG_DISABLE)) {
		wdg_enable();
	}
	
	/* Infinite loop */
	while (1)
	{
		eth_periodic();

		systime_periodic();

		led_periodic();

		//ee_config_periodic();

		wdg_feed();
	}
}

void netif_ext_status_callback(struct netif* nif, netif_nsc_reason_t reason, const netif_ext_callback_args_t* args)
{
	(void) args;
	(void) nif;

	if (reason & (LWIP_NSC_IPV4_ADDRESS_CHANGED | LWIP_NSC_IPV4_GATEWAY_CHANGED | LWIP_NSC_IPV4_NETMASK_CHANGED | LWIP_NSC_IPV4_SETTINGS_CHANGED)) {
//		console_telnet_restart();
	}
}
