#ifndef EE_CONFIG_h_
#define EE_CONFIG_h_

#include "platform.h"
#include "console.h"

#include "config.h"
#include "lwip/ip_addr.h"

enum ee_config_state_t {
	EE_OK,
	EE_NOINIT,
	EE_NOEEPROM,
	EE_NOMAGIC,
	EE_BADCHKSUM,
};

enum ee_config_savestate_t {
	EE_SAVE_IDLE,
	EE_SAVE_SAVE,
	EE_SAVE_WRITING,
	EE_SAVE_WAITING,
	EE_SAVE_VERIFY,
	EE_SAVE_ERROR,
};

// change this, when you change ee_config_t struct
#define EE_CFG_MAGIC       0xCAFA

#define EE_FLAGS(x)        (ee_config.flags & (1 << (x)))
#define EE_FLAGS_DHCP          0
#define EE_FLAGS_AUTONEG       1
#define EE_FLAGS_100M          2
#define EE_FLAGS_DUPLEX        3
#define EE_FLAGS_MQTT_DISCOVERY 4
#define EE_FLAGS_5  5
#define EE_FLAGS_6  7
#define EE_FLAGS_7  7
#define EE_FLAGS_WDG_DISABLE   8

enum {
	TYPE_LABEL,
	TYPE_BIT,
	TYPE_NUM,
	TYPE_SNUM,
	TYPE_IPADDR,
	TYPE_ARRAY,
	TYPE_STRING,
	TYPE_PASSWD,
};

union anyvalue {
	uint8_t bytes[4];
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	int8_t s8;
	int16_t s16;
	int32_t s32;
	void *ptr;
	struct ip4_addr ipaddr;
};

struct config_desc_t {
	uint32_t offset;
	uint8_t type;
	uint32_t shift;
	uint32_t param;
	char *str;
	char *comment;
};

struct ee_config_server_t {
	uint16_t active;
	struct ip4_addr addr;
	uint16_t port;
} PACK;

struct ee_config_t {
// 0x00
	uint16_t magic;            // 2
	uint8_t cfg_ver;           // 1

	uint16_t flags;            // 2
	uint8_t server_num;        // 1
	uint8_t uart_speed;        // 1
	uint8_t uart_verbosity;
// 0x08
	struct ip4_addr addr;       // 4
	struct ip4_addr netmask;    // 4
// 0x10
	struct ip4_addr gw;         // 4

	char username[EE_USERNAME_LEN];
	char password[EE_PASSWORD_LEN];

	uint16_t discovery_port;

	struct ee_config_server_t sntp_server;

	uint8_t can_speed;

	struct ee_config_server_t mqtt_server;  // = 1*(2+4+2)
	char mqtt_user[EE_USERNAME_LEN];
	char mqtt_pass[EE_PASSWORD_LEN];
	uint8_t relay_count;

	char mqtt_main_topic[30];
	char mqtt_relay_topic[30];
	char mqtt_input_topic[30];

	char mqtt_discovery_topic[30];
	char mqtt_output0_name[10];
	char mqtt_output1_name[10];
	char mqtt_output2_name[10];
	char mqtt_output3_name[10];
	char mqtt_output4_name[10];
	char mqtt_output5_name[10];
	char mqtt_output6_name[10];
	char mqtt_output7_name[10];

	char mqtt_input0_name[10];
	char mqtt_input1_name[10];
	char mqtt_input2_name[10];
	char mqtt_input3_name[10];
	char mqtt_input4_name[10];
	char mqtt_input5_name[10];
	char mqtt_input6_name[10];
	char mqtt_input7_name[10];

	uint8_t free[0x300];
	
	// checksum needs to be last
	uint8_t mac[6];
	uint16_t chksum;
} PACK;

extern struct ee_config_t ee_config;
extern struct ee_config_t ee_config_default;
extern uint8_t ee_config_state;
extern volatile uint8_t ee_config_dosave;

uint8_t ee_config_init(void);
char *ee_config_get_state_str(void);
uint8_t ee_config_get_status(void);
char *ee_config_get_save_state_str(void);
void ee_config_apply(struct ee_config_t *config);
void ee_config_save(struct ee_config_t *config);
void ee_config_periodic(void);
void ee_config_app_restart(void);

void ee_config_print(struct console_session_t *cs, struct config_desc_t *config_desc, void *data);
int ee_config_alter(struct config_desc_t *config_desc, void *data, char *param, char *value);

#endif
