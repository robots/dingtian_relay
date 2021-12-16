#include "platform.h"

#include "gpio.h"
#include "bsp_console_uart.h"

const struct gpio_init_table_t console_uart_gpio[] = {
	{
		.gpio = GPIOA,
		.pin = GPIO_Pin_9,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{
		.gpio = GPIOA,
		.pin = GPIO_Pin_10,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},
};

const int console_uart_gpio_cnt = ARRAY_SIZE(console_uart_gpio);
