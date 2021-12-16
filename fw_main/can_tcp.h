#ifndef CAN_h_
#define CAN_h_

#include "platform.h"

struct comfort_frame_t {
	uint8_t data[12];
};

void can_tcp_periodic(void);
void can_tcp_init(void);

#endif
