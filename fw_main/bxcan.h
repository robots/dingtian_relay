/*
 * bxCAN driver for STM32 family processors
 *
 * 2010 Michal Demin
 *
 */

#ifndef BXCAN_H_
#define BXCAN_H_

#include "fifo.h"

extern uint32_t CAN_Error;

struct can_timing_t {
	const char *baud;
	uint16_t brp; // brp[0:9]
	uint16_t ts; // res[15] lbkm[14] res[13:10] swj[9:8] res[7] ts2[6:4] ts1[3:0]
} __attribute__ ((packed));

#define CAN_MSG_SIZE  0x0F // DLC[0:3]
#define CAN_MSG_RTR   0x10 // RTR[4]
#define CAN_MSG_EID   0x20 // EID[5]
#define CAN_MSG_INV   0x80 // is message in-valid

struct can_message_t {
	uint32_t id;
	uint8_t flags;
	uint8_t data[8];
	uint16_t time;
} __attribute__ ((packed));

enum can_filter_mode_t {
	CAN_FILTER_IDMASK32,
	CAN_FILTER_ID32,
	CAN_FILTER_IDMASK16,
	CAN_FILTER_ID16,
};

struct can_filter_32_t {
	union {
		uint32_t raw;
		struct {
			uint32_t stid:11;
			uint32_t exid:18;
			uint32_t ide:1;
			uint32_t rtr:1;
			uint32_t zero:1;
		};
	};
};

struct can_filter_16_t {
	union {
		uint16_t raw;
		struct {
			uint16_t stid:11;
			uint16_t rtr:1;
			uint16_t ide:1;
			uint16_t exid:3;
		};
	};
};

struct can_filter_t {
	uint8_t id; // filter id
	enum can_filter_mode_t mode;
  uint8_t fifo:1;
  uint8_t active:1;

	union {
		uint32_t raw[2];
		struct {
			struct can_filter_32_t id;
			struct can_filter_32_t mask;
		} idmask32;
		struct can_filter_32_t id32[2];
		struct {
			struct can_filter_16_t id;
			struct can_filter_16_t mask;
		} idmask16[2];
		struct can_filter_16_t id16[4];
	};
};

uint8_t bxcan_set_baudrate(uint8_t baud);
void bxcan_set_rx_fifo(struct fifo_t *fifo);
void bxcan_init();
void bxcan_hw_reinit(void);

uint8_t bxcan_rx_ne(void);

void bxcan_recv(void);
uint8_t bxcan_send(struct can_message_t *m);

void bxcan_filter_clear(void);
void bxcan_filter_addmask(uint16_t cobid, uint16_t cobid_mask, uint8_t prio);
void bxcan_filter_apply(void);

#endif

