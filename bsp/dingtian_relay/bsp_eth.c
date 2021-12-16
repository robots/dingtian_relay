#include "platform.h"

#include "gpio.h"
#include "bsp_eth.h"

#define BSP_ETH_REMAP 1

const struct gpio_init_table_t eth_init_gpio[] = {
};

const struct gpio_init_table_t eth_gpio[] = {
/*	{ // PHY_RST
		.gpio = GPIOA,
		.pin = GPIO_Pin_4,
		.mode = GPIO_MODE_OUT_PP,
		.speed = GPIO_SPEED_HIGH,
		.state = GPIO_SET,
	},*/
	{ // RMII_CRS_DV - MODE2
		.gpio = GPIOD,
		.pin = GPIO_Pin_8,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_RXD0
		.gpio = GPIOD,
		.pin = GPIO_Pin_9,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_RXD1
		.gpio = GPIOD,
		.pin = GPIO_Pin_10,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},
/*	{ // RMII_RXER - PHYAD0,
		.gpio = GPIOA,
		.pin = GPIO_Pin_6,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},*/
	{ // RMII_REF_CLK
		.gpio = GPIOA,
		.pin = GPIO_Pin_1,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_MDIO
		.gpio = GPIOA,
		.pin = GPIO_Pin_2,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_MDC
		.gpio = GPIOC,
		.pin = GPIO_Pin_1,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_TXEN
		.gpio = GPIOB,
		.pin = GPIO_Pin_11,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_TXD0
		.gpio = GPIOB,
		.pin = GPIO_Pin_12,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{ // RMII_TXD1
		.gpio = GPIOB,
		.pin = GPIO_Pin_13,
		.mode = GPIO_MODE_AF_PP,
		.speed = GPIO_SPEED_HIGH,
	},
/*	{ // PHY_NINT
		.gpio = GPIOA,
		.pin = GPIO_Pin_5,
		.mode = GPIO_MODE_IN_FLOATING,
		.speed = GPIO_SPEED_HIGH,
	},*/
};

const int eth_init_gpio_cnt = ARRAY_SIZE(eth_init_gpio);
const int eth_gpio_cnt = ARRAY_SIZE(eth_gpio);
const int eth_gpio_remap = BSP_ETH_REMAP;
