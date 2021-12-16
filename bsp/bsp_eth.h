#ifndef BSP_ETH_h_
#define BSP_ETH_h_

#include "gpio.h"

extern const struct gpio_init_table_t eth_init_gpio[];
extern const struct gpio_init_table_t eth_gpio[];

extern const int eth_init_gpio_cnt;
extern const int eth_gpio_cnt;
extern const int eth_gpio_remap;

#endif
