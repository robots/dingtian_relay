#include "platform.h"
#include "config.h"

#include "rtc_f1.h"

#define RTC_LSB_Mask     ((uint32_t)0x0000FFFF)  /*!< RTC LSB Mask */

void rtc_f1_init()
{
	uint32_t p;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;

	// Allow access to BKP Domain
	PWR->CR |= PWR_CR_DBP;

	// Reset Backup Domain
  RCC->BDCR |= RCC_BDCR_BDRST;
  RCC->BDCR &= ~RCC_BDCR_BDRST;

#if HAVE_RTC_XTAL == 1
	// Enable the LSE OSC
  RCC->BDCR |= RCC_BDCR_LSEON;

	// Disable the LSI OSC
  RCC->CSR_LSION &= ~RCC_CSR_LSION;

	// Select the RTC Clock Source
  RCC->BDCR |= 1 << 8;
#else
	// Disable the LSI OSC
  RCC->CSR |= RCC_CSR_LSION;

	// Select the RTC Clock Source
  RCC->BDCR |= 2 << 8;
#endif
	// Enable the RTC Clock
  RCC->BDCR |= RCC_BDCR_RTCEN;

	// Wait for RTC registers synchronization
  RTC->CRL &= (uint16_t)~RTC_CRL_RSF;
  while (!(RTC->CRL & RTC_CRL_RSF)) {}

	// Wait until last write operation on RTC registers has finished
  while (!(RTC->CRL & RTC_CRL_RTOFF)) {}

	// Enable the RTC overflow interrupt
  //RTC->CRH |= RTC_CRH_OWIE;

#if HAVE_RTC_XTAL == 1
	//Set 32768 prescaler - for one second interupt
	p = (32768-1);
#else
	//Set 40kHz prescaler - for one second interupt
	p = (40000-1);
#endif

  while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
  RTC->CRL |= RTC_CRL_CNF;

  RTC->PRLH = (p & 0xf0000) >> 16;
  RTC->PRLL = (p & 0xffff);

  while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
  RTC->CRL &= ~RTC_CRL_CNF;
/*
	NVIC_EnableIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 1);
*/

}

uint32_t rtc_f1_get_time()
{
  uint16_t tmp = 0;
  tmp = RTC->CNTL;
  return (((uint32_t)RTC->CNTH << 16 ) | tmp) ;
}

void rtc_f1_set_time(uint32_t t)
{
  while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
  RTC->CRL |= RTC_CRL_CNF;

  RTC->CNTH = t >> 16;
  RTC->CNTL = (t & RTC_LSB_Mask);

  while (!(RTC->CRL & RTC_CRL_RTOFF)) {}
  RTC->CRL &= ~RTC_CRL_CNF;
}

void RTC_IRQHandler(void)
{
  RTC->CRL &= (uint16_t)~(RTC_CRL_OWF | RTC_CRL_ALRF | RTC_CRL_SECF);
/*
	if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == Bit_RESET) {
		GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
		GPIO_WriteBit(GPIOB, GPIO_Pin_14, Bit_RESET);
	} else {
		GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
		GPIO_WriteBit(GPIOB, GPIO_Pin_14, Bit_SET);
	}
	*/
}

