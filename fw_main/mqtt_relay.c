#include "platform.h"
#include <string.h>

#include <stdlib.h>
#include "console.h"
#include "ee_config.h"
#include "systime.h"
#include "printf.h"

#include "lwip/apps/mqtt.h"
#include "relay.h"

#include "mqtt_relay.h"

static mqtt_client_t* mqtt_relay_client;
static ip_addr_t mqtt_relay_ip;
static struct mqtt_connect_client_info_t mqtt_relay_ci =
{
  "mqtt_relay",
  NULL, /* user */
  NULL, /* pass */
  100,  /* keep alive */
  NULL, /* will_topic */
  NULL, /* will_msg */
  0,    /* will_qos */
  0     /* will_retain */
#if LWIP_ALTCP && LWIP_ALTCP_TLS
  , NULL
#endif
};

enum {
	MQTT_INIT,
	MQTT_NOTSET,
	MQTT_IDLE,
	MQTT_CONNECTING,
	MQTT_CONNECTED,
	MQTT_ERROR,
	MQTT_WAIT,
};


int mqtt_relay_set = -1;
int mqtt_state;
uint32_t mqtt_state_time;

static void mqtt_relay_statechange_cb(uint8_t type, uint8_t which, uint8_t state);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

void mqtt_relay_init(void)
{
	relay_init();
	relay_set_callback(mqtt_relay_statechange_cb);

	mqtt_relay_ci.client_user = ee_config.mqtt_user;
	mqtt_relay_ci.client_pass = ee_config.mqtt_pass;
	ip_addr_copy_from_ip4(mqtt_relay_ip, ee_config.mqtt_server.addr);

	if (ee_config.mqtt_server.active == 0) {
		mqtt_state = MQTT_NOTSET;
		return;
	}

	mqtt_state = MQTT_WAIT;
	mqtt_state_time = systime_get();

}

static void mqtt_connect(void)
{
	if (mqtt_relay_client != NULL) {
		mqtt_client_free(mqtt_relay_client);
		mqtt_relay_client = NULL;
	}

	if (mqtt_relay_client == NULL) {
		mqtt_relay_client = mqtt_client_new();
	}

  err_t e = mqtt_client_connect(mqtt_relay_client, &mqtt_relay_ip, MQTT_PORT, mqtt_connection_cb, LWIP_CONST_CAST(void*, &mqtt_relay_ci), &mqtt_relay_ci);
  mqtt_set_inpub_callback(mqtt_relay_client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, LWIP_CONST_CAST(void*, &mqtt_relay_ci));

	if (e == ERR_OK) {
		mqtt_state = MQTT_CONNECTING;
		mqtt_state_time = systime_get();

		console_printf(CON_ERR, "mqtt connect OK\n");
		return;
	}

	console_printf(CON_ERR, "mqtt connect e=%d\n", e);

	mqtt_state = MQTT_WAIT;
	mqtt_state_time = systime_get();
}

void mqtt_relay_periodic(void)
{
	relay_periodic();

	if (mqtt_state == MQTT_WAIT) {
		if (systime_get() - mqtt_state_time > SYSTIME_SEC(2)) {
			mqtt_connect();
		}
	}
}

static void mqtt_relay_statechange_cb(uint8_t type, uint8_t which, uint8_t state)
{
	char topic[100];
	const char *p;
	const char *t;

	if (type == RELAY_INPUT) {
		t = ee_config.mqtt_input_topic;
	} else if (type == RELAY_OUTPUT) {
		t = ee_config.mqtt_relay_topic;
	} else {
		return;
	}

	// generates "main_topic/input_or_relay_X/state" string
	tfp_sprintf(topic, "%s/%s%d/state", ee_config.mqtt_main_topic, t, which);

	if (state == 0) {
		p = "OFF";
	} else {
		p = "ON";
	}
	
	err_t e = mqtt_publish(mqtt_relay_client, topic, p, strlen(p), 0, 0, NULL, NULL);		
	console_printf(CON_INFO, "MQTT client \"%s\" publis: topic '%s' data '%s' ret = %d\n", mqtt_relay_ci.client_id, topic, p, e);
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;
	LWIP_UNUSED_ARG(data);

	console_printf(CON_INFO, "MQTT client \"%s\" data cb: len %d, flags %d\n", client_info->client_id, (int)len, (int)flags);

	// match relay command
	if (mqtt_relay_set >= 0) {
		if (strncasecmp((const char *)data, "OFF", 3) == 0) {
			relay_set(mqtt_relay_set, 0);
			console_printf(CON_WARN, "MQTT client \"%s\" relay %d set OFF\n", client_info->client_id, mqtt_relay_set);
		} else if (strncasecmp((const char *)data, "ON", 2) == 0) {
			relay_set(mqtt_relay_set, 1);
			console_printf(CON_WARN, "MQTT client \"%s\" relay %d set ON\n", client_info->client_id, mqtt_relay_set);
		}
	}
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;

	console_printf(CON_INFO, "MQTT client \"%s\" publish cb: topic %s, len %d\n", client_info->client_id, topic, (int)tot_len);
	
	const char *p;
	char t[100];
	int i;
	for (i = 0; i < ee_config.relay_count; i ++) {
		// looking for "relayX/set" string in incomming topic
		tfp_sprintf(t, "%s%d/set", ee_config.mqtt_relay_topic, i);

		p = strstr(topic, t);
		
		// found
		if (p != NULL)
			break;
	}

	if (i == ee_config.relay_count) {
		// not found
		mqtt_relay_set = -1;
		return;
	}

	// set global relay id, data will follow
	mqtt_relay_set = i;
	console_printf(CON_INFO, "MQTT client \"%s\" relay %d\n", client_info->client_id, i);
}

static void mqtt_request_cb(void *arg, err_t err)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;

  console_printf(CON_ERR, "MQTT client \"%s\" request cb: err %d\n", client_info->client_id, (int)err);
	if (err != ERR_OK) {
		// something wrong happened, restart client
		console_printf(CON_ERR, "MQTT client \"%s\" restarting client\n", client_info->client_id);
		mqtt_state = MQTT_WAIT;
		mqtt_state_time = systime_get();
	}
}

// unique id generation stolen from libusb32 stm32: https://github.com/dmitrystu/libusb_stm32
static uint32_t fnv1a32_turn (uint32_t fnv, uint32_t data ) {
    for (int i = 0; i < 4 ; i++) {
        fnv ^= (data & 0xFF);
        fnv *= 16777619;
        data >>= 8;
    }
    return fnv;
}

static void get_serialno(char *str) {
    uint32_t fnv = 2166136261;
    fnv = fnv1a32_turn(fnv, *(uint32_t*)(UID_BASE + 0x00));
    fnv = fnv1a32_turn(fnv, *(uint32_t*)(UID_BASE + 0x04));
    fnv = fnv1a32_turn(fnv, *(uint32_t*)(UID_BASE + 0x08));

    for (int i = 28; i >= 0; i -= 4 ) {
        uint16_t c = (fnv >> i) & 0x0F;
        c += (c < 10) ? '0' : ('A' - 10);
        *str++ = c;
    }
}

static void send_discovery_json(mqtt_client_t *client, uint8_t type, uint8_t idx)
{
	char *out;
	uint32_t len;
	char topic[100];
	const char *t;
	const char *tt;

	if (type == RELAY_INPUT) {
		t = "binary_sensor";
		tt = ee_config.mqtt_input_topic;
	} else if (type == RELAY_OUTPUT) {
		t = "switch";
		tt = ee_config.mqtt_relay_topic;
	} else {
		return;
	}
	tfp_sprintf(topic, "%s/%s/%s_%s%d/set", ee_config.mqtt_discovery_topic, t, tt, idx);

	out = malloc(200);
	len = 0;

	len += tfp_sprintf(out+len, "{ ");

	if (type == RELAY_OUTPUT) {
		len += tfp_sprintf(out+len, " \"command_topic\": \"%s/%s%d/set\", ", ee_config.mqtt_main_topic, t, idx);
	}

	len += tfp_sprintf(out+len, "\"state_topic\": \"%s/%s%d/state\", ", ee_config.mqtt_main_topic, t, idx);

	{ 
		char uid[9];
		memset(uid, 0, 9);
		get_serialno(uid);
		len += tfp_sprintf(out+len, "\"unique_id\": \"%s\", ", uid);
	}

	len += tfp_sprintf(out+len, " }");
	
	err_t e = mqtt_publish(client, topic, out, strlen(out), 0, 0, NULL, NULL);		
	free(out);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
  const struct mqtt_connect_client_info_t* client_info = (const struct mqtt_connect_client_info_t*)arg;
  LWIP_UNUSED_ARG(client);

	mqtt_state = MQTT_IDLE;
	mqtt_state_time = systime_get();

  console_printf(CON_INFO, "MQTT client \"%s\" connection cb: status %d\n", client_info->client_id, (int)status);

  if (status == MQTT_CONNECT_ACCEPTED) {
		mqtt_state = MQTT_CONNECTED;

		// subscribe to all relay topics
		for (int i = 0; i < ee_config.relay_count; i++) {
			char topic[100];

			tfp_sprintf(topic, "%s/%s%d/set", ee_config.mqtt_main_topic, ee_config.mqtt_relay_topic, i);
			console_printf(CON_INFO, "MQTT client \"%s\" subscribing to '%s'\n", client_info->client_id, topic);
			mqtt_sub_unsub(client, topic, 0, mqtt_request_cb, LWIP_CONST_CAST(void*, client_info), 1);

			if (EE_FLAGS(EE_FLAGS_MQTT_DISCOVERY)) {
				send_discovery_json(client, RELAY_INPUT, i);
				send_discovery_json(client, RELAY_OUTPUT, i);
			}
		}

  } else if (status == MQTT_CONNECT_REFUSED_PROTOCOL_VERSION) {
		console_printf(CON_ERR, "MQTT client \"%s\" refused protocol version\n", client_info->client_id);
	} else if (status == MQTT_CONNECT_REFUSED_IDENTIFIER) {
		console_printf(CON_ERR, "MQTT client \"%s\" refused identifier\n", client_info->client_id);
	} else if (status == MQTT_CONNECT_REFUSED_SERVER) {
		console_printf(CON_ERR, "MQTT client \"%s\" refused server\n", client_info->client_id);
	} else if (status == MQTT_CONNECT_REFUSED_USERNAME_PASS) {
		console_printf(CON_ERR, "MQTT client \"%s\" refused user/pass\n", client_info->client_id);
	} else if (status == MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_) {
		console_printf(CON_ERR, "MQTT client \"%s\" not authorized\n", client_info->client_id);
	}
}
