#include "platform.h"

#include "gpio.h"
#include "bsp_can.h"

const struct gpio_init_table_t can_gpio[] = {
	{
		.gpio = GPIOB,
		.pin = GPIO_Pin_8,
		.mode = GPIO_MODE_IPU,
		.speed = GPIO_SPEED_HIGH,
	},
	{
		.gpio = GPIOB,
		.pin = GPIO_Pin_9,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
};

const uint32_t can_gpio_remap = 2;

const uint32_t can_gpio_cnt = ARRAY_SIZE(can_gpio);

