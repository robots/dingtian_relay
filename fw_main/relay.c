#include "platform.h"
#include <string.h>

#include <stdlib.h>
#include "console.h"
#include "ee_config.h"
#include "systime.h"

#include "relay.h"

#include "bsp_relay.h"

#define RELAY_MASK ((1 << (ee_config.relay_count+1))-1) 
const struct gpio_init_table_t relay_gpio[] = {
	{
		.gpio = GPIOE,
		.pin = 0xFF00,
		.mode = GPIO_MODE_OUT_PP,
		.speed = GPIO_SPEED_HIGH,
	},
	{
		.gpio = GPIOE,
		.pin = 0x00FF,
		.mode = GPIO_MODE_IPU,
		.speed = GPIO_SPEED_HIGH,
	},
};

static struct console_command_t relay_cmd;
static uint32_t last_time;
static uint32_t last_in;
static uint32_t last_out;
relay_cb_t relay_cb;

void relay_init(void)
{
	gpio_init(relay_gpio, ARRAY_SIZE(relay_gpio));

	last_time = 0;
	last_in = relay_get_in();
	last_out = relay_get_out();

	console_add_command(&relay_cmd);
}

void relay_periodic(void)
{
	if (systime_get() - last_time > SYSTIME_SEC(1)) {
		last_time = systime_get();

		{
			uint32_t in = relay_get_in();
			uint32_t in_ch = in ^ last_in;

			if (in_ch) {
				for (int i = 0; i < ee_config.relay_count; i++) {
					if (in_ch & (1 << i)) {
						if (relay_cb) relay_cb(RELAY_INPUT, i, (in >> i) & 1);
					}
				}
			}
			last_in = in;
		}

		{
			uint32_t out = relay_get_out();
			uint32_t out_ch = out ^ last_out;
			if (out_ch) {
				for (int i = 0; i < ee_config.relay_count; i++) {
					if (out_ch & (1 << i)) {
						if (relay_cb) relay_cb(RELAY_OUTPUT, i, (out >> i) & 1);
					}
				}
			}
			last_out = out;
		}
	}
}

void relay_set_callback(relay_cb_t cb)
{
	relay_cb = cb;
}

uint16_t relay_get_in(void)
{
	return (~(GPIOE->IDR & 0xff)) & RELAY_MASK;
}

uint16_t relay_get_out(void)
{
	return (GPIOE->IDR >> 8) & 0xff & RELAY_MASK;
}

void relay_set(uint8_t relay, uint8_t state)
{
	relay_set_out(1 << relay, state);
}

void relay_set_out(uint8_t bits, uint8_t state)
{
	if (state) {
		GPIOE->BSRR |= bits << 8;
	} else {
		GPIOE->BRR |= bits << 8;
	}
/*
	for (int i = 0; i < ee_config.relay_count; i++) {
		if (bits & (1 << i)) {
			if (relay_cb) relay_cb(RELAY_OUTPUT, i, !!state);
		}
	}*/
}

static uint8_t relay_cmd_handler(struct console_session_t *cs, char **args)
{
	uint8_t ret = 1;

	if (args[0] == NULL) {
		uint16_t x;

		console_session_printf(cs, "Inputs:\n");
		x = relay_get_in();
		for (int i = 0; i < ee_config.relay_count; i++) {
			console_session_printf(cs, " %d = %d\n", i, (x >> i) & 1);
		}

		console_session_printf(cs, "Outputs:\n");
		x = relay_get_out();
		for (int i = 0; i < ee_config.relay_count; i++) {
			console_session_printf(cs, " %d = %d\n", i, (x >> i) & 1);
		}
	} else if (strcmp(args[0], "set") == 0) {
		uint8_t x = atoi(args[1]);
		uint8_t y = atoi(args[2]);
		
		relay_set_out(1 << x, y);
	}

	return ret;
}

static struct console_command_t relay_cmd = {
	"relay",
	relay_cmd_handler,
	"relay menu",
	"relay state - get state of relays and inputs\nrelay set x y - set output x to y",
	NULL
};
