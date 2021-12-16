#include "platform.h"

#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "flash.h"
#include "utils.h"
#include "ee_config.h"
#include "systime.h"

#define EE_CONFIG_ADDR ((uint32_t)0x08040000-0x800)

struct ee_config_t ee_config_default = {
	.flags = BV(EE_FLAGS_DHCP) | BV(EE_FLAGS_AUTONEG),
	.uart_speed = 0,
	.uart_verbosity = 0,

	.username = "admin",
	.password = "password",
#if HAVE_SNTP
	.sntp_server = {0, {0}, 123},
#endif

	.relay_count = 4,
	.mqtt_discovery_topic = "homeassistant",
	.mqtt_main_topic = "dingtian",
	.mqtt_relay_topic = "relay",
	.mqtt_input_topic = "input",
};

struct ee_config_t ee_config;
struct ee_config_t console_eeconfig;
uint8_t ee_config_state = EE_NOINIT;

volatile uint8_t ee_config_dosave;
struct ee_config_t *ee_config_tosave;

static struct console_command_t ee_config_cmd;
static uint16_t ee_config_chksum(struct ee_config_t *ee, uint32_t len);

uint8_t ee_config_init(void)
{
	STATIC_ASSERT((sizeof(struct ee_config_t) < 0x800), "Bad size of struct ee_config_t!!")

	ee_config_dosave = 0;

	memcpy(&ee_config, (void *)EE_CONFIG_ADDR, sizeof(struct ee_config_t));

	if (ee_config.magic == EE_CFG_MAGIC) {
		if (ee_config_chksum(&ee_config, sizeof(struct ee_config_t)) == ee_config.chksum) {
			ee_config_state = EE_OK;
		} else {
			ee_config_state = EE_BADCHKSUM;
		}
	} else {
		ee_config_state = EE_NOMAGIC;
	}

	if (ee_config_state != EE_OK) {
		// create some random (yet unique) MAC address
		uint32_t Device_Serial0, Device_Serial1, Device_Serial2;
		Device_Serial0 = *(volatile uint32_t*)(0x1FFFF7E8);
		Device_Serial1 = *(volatile uint32_t*)(0x1FFFF7EC);
		Device_Serial2 = *(volatile uint32_t*)(0x1FFFF7F0);
		Device_Serial0 += Device_Serial2;

		memcpy(&ee_config.mac[0], &Device_Serial0, 4);
		memcpy(&ee_config.mac[4], &Device_Serial1, 2);

		// locally admin address
		ee_config.mac[0] = 0x02;
	}

	if (ee_config_state != EE_OK) {
		// copy default except mac addr and checksum
		memcpy(&ee_config, &ee_config_default, sizeof(struct ee_config_t)-8);
	}

	console_eeconfig = ee_config;
	console_add_command(&ee_config_cmd);

	return ee_config_state;
}

char *ee_config_state_name[] = {
	"OK",
	"Not inited",
	"No EEPROM found",
	"No MAGIC found in eeprom",
	"Bad checksum in eeprom",
};

char *ee_config_get_state_str(void)
{
	if (ee_config_state >= ARRAY_SIZE(ee_config_state_name))
		return NULL;

	return ee_config_state_name[ee_config_state];
}

uint8_t ee_config_get_status(void)
{
	return ee_config_state;
}

char *ee_config_dosave_name[] = {
	"IDLE",
	"Preparing",
	"Writing",
	"Waiting",
	"Verifying",
	"Error!",
};

char *ee_config_get_save_state_str(void)
{
	if (ee_config_dosave >= ARRAY_SIZE(ee_config_dosave_name))
		return NULL;

	return ee_config_dosave_name[ee_config_dosave];
}

void ee_config_apply(struct ee_config_t *config)
{
	ee_config = *config;

	ee_config_app_restart();
}

void ee_config_save(struct ee_config_t *config)
{
	if (ee_config_state == EE_NOEEPROM) {
		// really save ? to where ?
		return;
	}

	if (ee_config_dosave) {
		// we really need to wait for ee_config_dosave == 0, but not here
		return;
	}

	ee_config_tosave = config;
	ee_config_dosave = EE_SAVE_SAVE;
}

void ee_config_periodic(void)
{
	if ((ee_config_dosave > 0) && (ee_config_dosave != EE_SAVE_ERROR)) {

		ee_config_tosave->magic = EE_CFG_MAGIC;
		ee_config_tosave->chksum = ee_config_chksum(ee_config_tosave, sizeof(struct ee_config_t));

		__disable_irq();

		uint32_t addr = 0;

		flash_unlock();
		flash_erase_page(EE_CONFIG_ADDR);
		flash_lock();

		while (addr < sizeof(struct ee_config_t)) {
			uint16_t t;

			flash_unlock();

			memcpy(&t, (void *)((uint32_t)ee_config_tosave+addr), 2);
			flash_program((uint32_t)EE_CONFIG_ADDR + addr, t);
			addr += 2;

			flash_lock();
		}

		__enable_irq();

		struct ee_config_t *tmp_cfg = (struct ee_config_t *)EE_CONFIG_ADDR;
		if ((tmp_cfg->magic == EE_CFG_MAGIC) && (ee_config_chksum((struct ee_config_t *)tmp_cfg, sizeof(struct ee_config_t)) == tmp_cfg->chksum) &&
			memcmp(tmp_cfg, ee_config_tosave, sizeof(struct ee_config_t)) == 0) {
			
			ee_config_dosave = EE_SAVE_IDLE;
		} else {
			ee_config_dosave = EE_SAVE_ERROR;
		}
	}
}

static uint16_t ee_config_chksum(struct ee_config_t *ee, uint32_t len)
{
	uint8_t *data = (uint8_t *)ee;
	uint16_t sum = 0;

	for (uint32_t i = 0; i < len - 2; i++) {
		sum += data[i];
	}

	return sum;
}

#if CONSOLE_DISABLE == 0
void ee_config_print(struct console_session_t *cs, struct config_desc_t *cd, void *data)
{
	char out[CONSOLE_CMD_OUTBUF];
	uint32_t len = 0;
	union anyvalue val;

	while (cd->str) {
		memcpy(val.bytes, (void *)((uint32_t)data+cd->offset), 4);

		if (cd->type == TYPE_LABEL) {
			len = tfp_sprintf(out, "*** %s", cd->str);
		} else {
			len = tfp_sprintf(out, "%s = ", cd->str);

			if (cd->type == TYPE_IPADDR) {
				len += tfp_sprintf(out+len, "%A", val.u32);
			} else if (cd->type == TYPE_NUM) {
				uint32_t num = 0;
				if (cd->param == 8) {
					num = val.u8;
				} else if (cd->param == 16) {
					num = val.u16;
				} else if (cd->param == 32) {
					num = val.u32;
				}
				len += tfp_sprintf(out+len, "%d", num);
			} else if (cd->type == TYPE_SNUM) {
				int32_t num = 0;
				if (cd->param == 8) {
					num = val.s8;
				} else if (cd->param == 16) {
					num = val.s16;
				} else if (cd->param == 32) {
					num = val.s32;
				}
				len += tfp_sprintf(out+len, "%d", num);
			} else if (cd->type == TYPE_BIT) {
				uint8_t state = 0;
				if (cd->param == 8) {
					state = val.u8 & (1 << cd->shift);
				} else if (cd->param == 16) {
					state = val.u16 & (1 << cd->shift);
				} else if (cd->param == 32) {
					state = val.u32 & (1 << cd->shift);
				}
				len += tfp_sprintf(out+len, "%d", !!state);
			} else if (cd->type == TYPE_ARRAY) {
				uint8_t *ary = (uint8_t *)((uint32_t)data + cd->offset);
				for (uint32_t j = 0; j < cd->param; j++) {
					len += tfp_sprintf(out+len, "%02x", ary[j]);
					if (j < cd->param-1)
						len += tfp_sprintf(out+len, ":");
				}
			} else if (cd->type == TYPE_STRING) {
				char *str = (char *)((uint32_t)data + cd->offset);
				for (uint32_t j = 0; j < cd->param; j++) {
					if (str[j] == '\0') {
						break;
					}
					len += tfp_sprintf(out+len, "%c", str[j]);
				}
			} else if (cd->type == TYPE_PASSWD) {
				for (uint32_t j = 0; j < cd->param; j++) {
					len += tfp_sprintf(out+len, "*");
				}
			}
		}

		if (cd->comment) {
			len += tfp_sprintf(out+len, "\t%s", cd->comment);
		}

		len += tfp_sprintf(out+len, "\n");
		console_session_output(cs, out, len);
		cd++;
	}
}

int ee_config_alter(struct config_desc_t *config_desc, void *data, char *param, char *value)
{
	struct config_desc_t *cd = NULL;
	uint32_t i = 0;
	union anyvalue val;

	while (1) {
		if (config_desc[i].str == NULL) {
			return 2;
		}

		if (strcmp(param, config_desc[i].str) == 0) {
			cd = &config_desc[i];
			break;
		}
		i++;
	}

	if (cd == NULL) {
		return 2;
	}

	if ((cd->type == TYPE_NUM) || (cd->type == TYPE_SNUM) || (cd->type == TYPE_BIT)) {
		memcpy(val.bytes, (void *)((uint32_t)data+cd->offset), 4);
		if (cd->type == TYPE_NUM) {
			char *end;
			if (config_desc[i].param == 8) {
				val.u8 = strtol(value, &end, 0);
			} else if (config_desc[i].param == 16) {
				val.u16 = strtol(value, &end, 0);
			} else if (config_desc[i].param == 32) {
				val.u32 = strtol(value, &end, 0);
			}
			if (*end != 0) {
				//console_session_printf(cs, "Number parse error\n");
				return 1;
			}
		} else if (cd->type == TYPE_SNUM) {
			char *end;
			if (config_desc[i].param == 8) {
				val.s8 = strtol(value, &end, 0);
			} else if (config_desc[i].param == 16) {
				val.s16 = strtol(value, &end, 0);
			} else if (config_desc[i].param == 32) {
				val.s32 = strtol(value, &end, 0);
			}
			if (*end != 0) {
				//console_session_printf(cs, "Number parse error\n");
				return 1;
			}
		} else if (cd->type == TYPE_BIT) {
			uint8_t state = atoi(value)?1:0;
			uint32_t mask = (1 << config_desc[i].shift);
			if (config_desc[i].param == 8) {
				if (state) val.u8 |= mask; else	val.u8 &= ~mask;
			} else if (config_desc[i].param == 16) {
				if (state) val.u16 |= mask; else val.u16 &= ~mask;
			} else if (config_desc[i].param == 32) {
				if (state) val.u32 |= mask; else val.u32 &= ~mask;
			}
		}
		memcpy((void *)((uint32_t)data+cd->offset), val.bytes, 4);
		return 0;
	} else if (cd->type == TYPE_IPADDR) {
		parse_ipaddr(value, (struct ip4_addr *)((uint32_t)data + cd->offset));
		return 0;
	} else if (cd->type == TYPE_ARRAY) {
		//console_session_printf(cs, "Array alteration not implemented\n");
		return 3;
	} else if ((cd->type == TYPE_STRING) || (cd->type == TYPE_PASSWD)) {
		char *str = (char *)((uint32_t)data + config_desc[i].offset);
		memset(str, 0, cd->param);
		memcpy(str, value, MIN(strlen(value), cd->param));
		return 0;
	}

	return 1;
}

struct config_desc_t config_desc[] = {
	{                                  0,            TYPE_LABEL,    0,  0, "Flags", NULL},
	{offsetof(struct ee_config_t, flags),            TYPE_BIT,      EE_FLAGS_DHCP,         16, "dhcp", NULL},
	{offsetof(struct ee_config_t, flags),            TYPE_BIT,      EE_FLAGS_AUTONEG,      16, "eth_autoneg", NULL},
	{offsetof(struct ee_config_t, flags),            TYPE_BIT,      EE_FLAGS_100M,         16, "eth_speed100", NULL},
	{offsetof(struct ee_config_t, flags),            TYPE_BIT,      EE_FLAGS_DUPLEX,       16, "eth_fullduplex", NULL},
	{offsetof(struct ee_config_t, flags),            TYPE_BIT,      EE_FLAGS_WDG_DISABLE , 16, "wdg_disable", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "UART console", NULL},
	{offsetof(struct ee_config_t, uart_speed),       TYPE_NUM,      0,  8, "uart_speed", NULL},
	{offsetof(struct ee_config_t, uart_verbosity),   TYPE_NUM,      0,  8, "uart_verbosity", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "IPv4 setting", NULL},
	{offsetof(struct ee_config_t, addr),             TYPE_IPADDR,   0,  0, "addr", NULL},
	{offsetof(struct ee_config_t, netmask),          TYPE_IPADDR,   0,  0, "netmask", NULL},
	{offsetof(struct ee_config_t, gw),               TYPE_IPADDR,   1,  0, "gateway", NULL},
	{offsetof(struct ee_config_t, discovery_port),   TYPE_NUM,      1, 16, "discovery_port", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "MQTT Server", NULL},
	{offsetof(struct ee_config_t, mqtt_server.active), TYPE_NUM,      1, 16, "server_active", NULL},
	{offsetof(struct ee_config_t, mqtt_server.addr),   TYPE_IPADDR,   1, 16, "server_addr", NULL},
	{offsetof(struct ee_config_t, mqtt_server.port),   TYPE_NUM,      1, 16, "server_port", NULL},
	{offsetof(struct ee_config_t, mqtt_user),        TYPE_STRING,   0,  EE_USERNAME_LEN, "mqtt_user", NULL},
	{offsetof(struct ee_config_t, mqtt_pass),        TYPE_PASSWD,   0,  EE_PASSWORD_LEN, "mqtt_pass", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "MQTT topics", NULL},
	{offsetof(struct ee_config_t, mqtt_discovery_topic), TYPE_STRING,   0,  30, "mqtt_discovery_topic", NULL},
	{offsetof(struct ee_config_t, mqtt_main_topic),  TYPE_STRING,   0,  30, "mqtt_main_topic", NULL},
	{offsetof(struct ee_config_t, mqtt_relay_topic), TYPE_STRING,   0,  30, "mqtt_relay_topic", NULL},
	{offsetof(struct ee_config_t, mqtt_input_topic), TYPE_STRING,   0,  30, "mqtt_input_topic", NULL},
	{offsetof(struct ee_config_t, mqtt_output0_name), TYPE_STRING,  0,  10, "output0_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output1_name), TYPE_STRING,  0,  10, "output1_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output2_name), TYPE_STRING,  0,  10, "output2_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output3_name), TYPE_STRING,  0,  10, "output3_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output4_name), TYPE_STRING,  0,  10, "output4_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output5_name), TYPE_STRING,  0,  10, "output5_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output6_name), TYPE_STRING,  0,  10, "output6_name", NULL},
	{offsetof(struct ee_config_t, mqtt_output7_name), TYPE_STRING,  0,  10, "output7_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input0_name), TYPE_STRING,   0,  10, "input0_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input1_name), TYPE_STRING,   0,  10, "input1_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input2_name), TYPE_STRING,   0,  10, "input2_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input3_name), TYPE_STRING,   0,  10, "input3_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input4_name), TYPE_STRING,   0,  10, "input4_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input5_name), TYPE_STRING,   0,  10, "input5_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input6_name), TYPE_STRING,   0,  10, "input6_name", NULL},
	{offsetof(struct ee_config_t, mqtt_input7_name), TYPE_STRING,   0,  10, "input7_name", NULL},

	{                                  0,            TYPE_LABEL,    0,  0, "DINGTIAN board setting", NULL},
	{offsetof(struct ee_config_t, relay_count),      TYPE_NUM,      1,  8, "relay_count", NULL},

	{                                  0,            TYPE_LABEL,    0,  0, "SNTP server setting", NULL},
	{offsetof(struct ee_config_t, sntp_server.active), TYPE_NUM,    1, 16, "sntp_active", NULL},
	{offsetof(struct ee_config_t, sntp_server.addr),   TYPE_IPADDR, 1, 16, "sntp_addr", NULL},
	{offsetof(struct ee_config_t, sntp_server.port),   TYPE_NUM,    1, 16, "sntp_port", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "Remote login", NULL},
	{offsetof(struct ee_config_t, username),         TYPE_STRING,   0,  EE_USERNAME_LEN, "login_user", NULL},
	{offsetof(struct ee_config_t, password),         TYPE_PASSWD,   0,  EE_PASSWORD_LEN, "login_pass", NULL},
	{                                  0,            TYPE_LABEL,    0,  0, "CAN", NULL},
	{offsetof(struct ee_config_t, can_speed),        TYPE_NUM,      0,  8, "can_speed", NULL},
	{offsetof(struct ee_config_t, mac),              TYPE_ARRAY,    0,  6, "mac", NULL},
	{                                  0,            0,             0,  0, NULL, NULL},
};

static uint8_t ee_config_cmd_handler(struct console_session_t *cs, char **args)
{
	if (args[0] == NULL) {
		console_session_printf(cs, "Valid: show, set {item} {val}, revert, default, apply, status\n");
		return 1;
	}

	if (strcmp(args[0], "show") == 0) {
		console_session_printf(cs, "Current configuration\n");
		ee_config_print(cs, config_desc, &console_eeconfig);

	} else if (strcmp(args[0], "set") == 0) {
		if ((args[1] == NULL) || (args[2] == NULL)) {
			return 1;
		}

		int alt = ee_config_alter(config_desc, &console_eeconfig, args[1], args[2]);

		if (alt == 0) {
			console_session_printf(cs, "Parameter '%s' altered\n", args[1]);
		} else if (alt == 1) {
			console_session_printf(cs, "Error setting parameter '%s'\n", args[1]);
		} else if (alt == 2) {
			console_session_printf(cs, "Parameter '%s' does not exists\n", args[1]);
		} else {
			console_session_printf(cs, "Not implemented\n");
		}

	} else if (strcmp(args[0], "revert") == 0) {
		console_eeconfig = ee_config;
	} else if (strcmp(args[0], "default") == 0) {
		console_eeconfig = ee_config_default;
	} else if (strcmp(args[0], "apply") == 0) {
		ee_config_apply(&console_eeconfig);
	} else if (strcmp(args[0], "save") == 0) {
		ee_config_save(&console_eeconfig);
	} else if (strcmp(args[0], "status") == 0) {
		console_session_printf(cs, "EEProm status: %s\n", ee_config_get_state_str());
		console_session_printf(cs, "EEProm statemachine %s\n", ee_config_get_save_state_str());
	}

	return 0;
}

static struct console_command_t ee_config_cmd = {
	"conf",
	ee_config_cmd_handler,
	"Configuration",
	"Configuration:\n revert - restore from eeprom\n default - restore from built-in defaults \n apply - apply current configuration\n save - save current configuration into NVMEM\n status - print current NVMEM status\n show - print current configuration \n set option value - sets option to value",
	NULL
};

void __attribute__ ((weak)) ee_config_app_restart(void)
{
}
#endif
