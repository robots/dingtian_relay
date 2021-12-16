#ifndef RELAY_h_
#define RELAY_h_

enum relay_type_t {
	RELAY_INPUT,
	RELAY_OUTPUT,
};

typedef void (*relay_cb_t)(uint8_t type, uint8_t which, uint8_t state);

void relay_init(void);
void relay_periodic(void);
void relay_set_callback(relay_cb_t cb);
uint16_t relay_get_in(void);
uint16_t relay_get_out(void);
void relay_set(uint8_t relay, uint8_t state);
void relay_set_out(uint8_t bits, uint8_t state);

#endif
