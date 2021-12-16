#ifndef BSP_RELAY_h_
#define BSP_RELAY_h_

#include "gpio.h"

extern const struct gpio_init_table_t relay_out_gpio[];
extern const struct gpio_init_table_t relay_in_gpio[];

extern const uint32_t relay_out_gpio_cnt;
extern const uint32_t relay_in_gpio_cnt;

#endif
