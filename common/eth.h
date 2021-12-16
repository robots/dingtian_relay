#ifndef ETH_h_
#define ETH_h_

#ifndef STM32_ETH_INTERRUPTS
#define STM32_ETH_INTERRUPTS 0
#endif

extern ETH_InitTypeDef ETH_InitStructure;

void eth_init();
void eth_periodic(void);
void eth_irq_enable(void);
void eth_irq_disable(void);

#endif
