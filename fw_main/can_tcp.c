#include "platform.h"

#include <string.h>
#include "lwip/tcp.h"

#include "config.h"
#include "console.h"
#include "systime.h"
#include "fifo.h"
#include "ee_config.h"

#include "lwip/mem.h"

#include "bxcan.h"
#include "can_tcp.h"
#include "utils.h"
#include "printf.h"

#define CAN_FLAG_CLOSING   1
#define CAN_FLAG_TIMEOUT   2

// timeout in seconds
#define CAN_TCP_TIMEOUT    5

#define CAN_RX_FIFO        10
#define CAN_TX_FIFO        10

//#define DEBUG 1
#define CAN_BUFFER 50

struct can_session_t {
	uint32_t active:1;

	struct tcp_pcb *pcb;

	uint32_t flags;

	uint8_t data[CAN_BUFFER];
	uint8_t data_idx;

	uint32_t last_recv;
};

struct fifo_t can_tcp_rx_fifo;
struct can_message_t can_tcp_rx_buffer[CAN_RX_FIFO];
struct fifo_t can_tcp_tx_fifo;
struct can_message_t can_tcp_tx_buffer[CAN_TX_FIFO];

//uint8_t can_init_data[] = {0x0d, 0x00, 0x0d, 0x01, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26};

struct can_session_t can_sessions[5];
//struct can_session_t *can_session = NULL;

static void print_cm(struct can_message_t *cm, int t)
{
	char buf[40];
	int len = 0;
	int i;

	len += tfp_sprintf(buf, "%03x %d ", cm->id, cm->flags & CAN_MSG_SIZE);
	for (i = 0; i < (cm->flags & CAN_MSG_SIZE); i++) {
		len += tfp_sprintf(&buf[len], "%02x ", cm->data[i]);
	}

	console_printf(CON_WARN, "can: %s %s\n", t==0?"recv":"send", buf);
}

static void can_tcp_cfresponse(struct comfort_frame_t *cf, int ok)
{
	int i;
	for (i = 0; i < 10; i++) {
		cf->data[i] = ok ? 0x55: 0xaa;
	}
}

static void can_tcp_cfchecksum(struct comfort_frame_t *cf)
{
	uint16_t sum = 0;
	int i;

	for (i = 0; i < 10; i++) {
		sum += cf->data[i];
	}

	cf->data[10] = sum >> 8;
	cf->data[11] = sum & 0xff;
}

static void can_tcp_send(struct can_session_t *cs, struct comfort_frame_t *cf)
{
	err_t err;
	char buf[50];
	int len = 0;
	int i;

	if (cs == NULL) {
		return;
	}

	memset(buf, 0, 50);

	can_tcp_cfchecksum(cf);

	//rewrite cf.data into ascii
	for (i = 0; i < 12; i++) {
		len += tfp_sprintf(&buf[len], "%02x", cf->data[i]);
	}
	len += tfp_sprintf(&buf[len], "\n");

#ifdef DEBUG
	console_printf(CON_INFO, "can: to client: %s\n", buf);
#endif

	err = tcp_write(cs->pcb, buf, len, TCP_WRITE_FLAG_COPY);
	if (err == ERR_MEM) {
		console_printf(CON_ERR, "can: tcp_write no mem\n");
	} else if (err != ERR_OK) {
		console_printf(CON_ERR, "can: tcp_write failed with err = %d\n", err);
	}
//	tcp_output(cs->pcb);
}

void can_tcp_process(struct can_session_t *cs, char c)
{
	uint32_t i;

	if (c == ' ')
		return;

	cs->data[cs->data_idx] = c;
	cs->data_idx++;

	if (cs->data_idx >= CAN_BUFFER - 1) {
		cs->data_idx = 0;
		return;
	}

	if (c == '\n' && cs->data_idx <= 12*2) {
		cs->data_idx = 0;
		return;
	}

	if (c == '\n' && cs->data_idx > 12*2) {
		struct comfort_frame_t cf;
		struct comfort_frame_t cfret;
		int retsend = 0;
		struct can_message_t *cm;

		if (FIFO_FULL(&can_tcp_tx_fifo)) {
			console_printf(CON_ERR, "can: tx fifo full\n");
			cs->data_idx = 0;
			return;
		}

		cm = fifo_get_write_addr(&can_tcp_tx_fifo);

#ifdef DEBUG
		console_printf(CON_INFO, "can: from client: %s\n", cs->data);
#endif

		// translate incomming data to comfort_frame
		for (i = 0; i < 12; i++) {
			cf.data[i] = 16 * hex2dec(cs->data[2*i+0]) + hex2dec(cs->data[2*i+1]);
		}

		// check checksum
		uint16_t sum = 0;
		for (i = 0; i < 10; i++) {
			sum += cf.data[i];
		}

		if (sum != (256*cf.data[10] + cf.data[11])) {
			console_printf(CON_ERR, "can: recv checksum error\n");
		}

		// got correct frame, send it to other clients
		for (i = 0; i < ARRAY_SIZE(can_sessions); i++) {
			if (can_sessions[i].active && cs != &can_sessions[i] && !(can_sessions[i].flags & CAN_FLAG_CLOSING)) {
				can_tcp_send(&can_sessions[i], &cf);
			}
		}

		// translate cf to can-message
		cm->flags = 7;
		cm->id = ((cf.data[0] & 0x0f) << 7) + (cf.data[1] & 0x7f);
		cm->data[0] = (cf.data[2] << 4) + (cf.data[3] & 0x0f);

		for (i = 0; i < 6; i++) {
			cm->data[i + 1] = cf.data[i + 4];
		}

		if (cf.data[3] == 0x01 || cf.data[3] > 1) {
			// get value
#ifdef DEBUG
			console_printf(CON_INFO, "can: get value\n");
#endif
			print_cm(cm, 1);
			fifo_write_done(&can_tcp_tx_fifo);
			// response will be generated upon receiving can frame
		} else if (cf.data[3] == 0x00) {
			// write value
#ifdef DEBUG
			console_printf(CON_INFO, "can: write value\n");
#endif
			print_cm(cm, 1);
			fifo_write_done(&can_tcp_tx_fifo);
			//generate ok response
			retsend = 1;
			can_tcp_cfresponse(&cfret, 1);
		}
/*
		else {
			// generate fail response
			retsend = 1;
			can_tcp_cfresponse(&cfret, 0);
			console_printf(CON_ERR, "can: unknown op\n");
		}
*/

		if (retsend) {
			can_tcp_send(cs, &cfret);
		}

		cs->data_idx = 0;
	}
}

static void can_tcp_close(struct tcp_pcb *pcb, struct can_session_t *cs)
{
	cs->flags |= CAN_FLAG_CLOSING;

	tcp_arg(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);

	if (tcp_close(pcb) != ERR_OK) {
		// not ready to close, retry from poll
		tcp_arg(pcb, cs);
	} else {
		// closing succeeded, we are done with this session
		cs->active = 0;
		cs->pcb = NULL;
	}
}

static err_t can_tcp_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct can_session_t *cs = (struct can_session_t *)arg;

	char c;
	uint32_t i;

	if (err != ERR_OK) {
		console_printf(CON_INFO, "can: recv err not ok err = %d\n", err);
		can_tcp_close(pcb, cs);
		return ERR_OK;
	}

	if (err == ERR_OK && p == NULL) {
#ifdef DEBUG
		console_printf(CON_DEBUG, "can: remote party quit\n", pcb->remote_ip.addr);
#endif
		can_tcp_close(pcb, cs);
		return ERR_OK;
	}

	if (err == ERR_OK && p != NULL) {

		tcp_recved(pcb, p->tot_len);

		cs->last_recv = systime_get();

		for (i = 0; i < p->tot_len; i++) {
			pbuf_copy_partial(p, &c, 1, i);
			//console_printf(CON_INFO, "can: process '%c'\n", c);
			can_tcp_process(cs, c);
		}

		pbuf_free(p);
	}

	return ERR_OK;
}

/*
static err_t can_tcp_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct can_session_t *cs = (struct can_session_t *)arg;

	LWIP_UNUSED_ARG(len);

	if (cs->data_left > 0) {
		send_data(pcb, cs);
	} else {
		if (cs->flags & CAN_FLAG_CLOSING) {
			can_tcp_close(pcb, cs);
		}
	}

	return ERR_OK;
}
*/

static void can_tcp_err(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(err);

	struct can_session_t *cs = (struct can_session_t *)arg;

	if (cs != NULL) {
		mem_free(cs);
	}

}

static err_t can_tcp_poll(void *arg, struct tcp_pcb *pcb)
{
	struct can_session_t *cs = (struct can_session_t *)arg;

	if (cs == NULL) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	if (cs->flags & CAN_FLAG_CLOSING) {
#ifdef DEBUG
		console_printf(CON_DEBUG, "can: closing from poll\n");
#endif
		can_tcp_close(pcb, cs);
		return ERR_OK;
	}

	if (cs->flags & CAN_FLAG_TIMEOUT) {
		if (systime_get() - cs->last_recv > SYSTIME_SEC(CAN_TCP_TIMEOUT)) {
			console_printf(CON_INFO, "can: timeout\n");

			can_tcp_close(pcb, cs);
		}
	}

	return ERR_OK;
}

static err_t can_tcp_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct can_session_t *cs = NULL;

	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

	tcp_setprio(pcb, TCP_PRIO_MIN);

#ifdef DEBUG
	console_printf(CON_INFO, "can: connection from %A\n", pcb->remote_ip.addr);
#endif

	for (uint32_t i = 0; i < ARRAY_SIZE(can_sessions); i++) {
		if (can_sessions[i].active == 0) {
			cs = &can_sessions[i];
			break;
		}
	}

	if (cs == NULL) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	cs->active = 1;
	cs->pcb = pcb;
	cs->last_recv = systime_get();
	cs->flags = CAN_FLAG_TIMEOUT;

	cs->data_idx = 0;

	tcp_arg(pcb, cs);
	tcp_recv(pcb, can_tcp_recv);
	//tcp_sent(pcb, can_tcp_sent);
	tcp_err(pcb, can_tcp_err);
	tcp_poll(pcb, can_tcp_poll, 4);

	return ERR_OK;
}

void can_tcp_periodic(void)
{
	/*
	if (bxcan_rx_ne()) {
		bxcan_recv();
	}*/

	if (!FIFO_EMPTY(&can_tcp_rx_fifo)) {
		struct can_message_t *cm;

		cm = fifo_get_read_addr(&can_tcp_rx_fifo);

#ifdef DEBUG
		console_printf(CON_INFO, "can: recved!\n");
#endif
		print_cm(cm, 0);

		struct comfort_frame_t cf;
		uint32_t i;

		cf.data[0] = (cm->id >> 7) & 0x0f;
		cf.data[1] = (cm->id & 0x7f); //0x0f);
		cf.data[2] = cm->data[0] >> 4;
		cf.data[3] = cm->data[0] & 0x0f;

		for (i = 0; i < 6; i++) {
			cf.data[i+4] = cm->data[i+1];
		}

		if ((cm->flags & CAN_MSG_SIZE) == 5) {
			cf.data[8] = 0x80;
			cf.data[9] = 0x08;
		}

		for (i = 0; i < ARRAY_SIZE(can_sessions); i++) {
			if (can_sessions[i].active && !(can_sessions[i].flags & CAN_FLAG_CLOSING)) {
				can_tcp_send(&can_sessions[i], &cf);
			}
		}

		fifo_read_done(&can_tcp_rx_fifo);
	}

	if (!FIFO_EMPTY(&can_tcp_tx_fifo)) {
		struct can_message_t *m = fifo_get_read_addr(&can_tcp_tx_fifo);

		if (bxcan_send(m) == 0) {
			// packet was written into bxcan's fifo
			fifo_read_done(&can_tcp_tx_fifo);
		}
	}
}

void can_tcp_init(void)
{
	struct tcp_pcb *pcb;

	fifo_init(&can_tcp_tx_fifo, can_tcp_tx_buffer, sizeof(struct can_message_t), CAN_TX_FIFO);
	fifo_init(&can_tcp_rx_fifo, can_tcp_rx_buffer, sizeof(struct can_message_t), CAN_RX_FIFO);

	// init can
	bxcan_set_rx_fifo(&can_tcp_rx_fifo);
	bxcan_init(ee_config.can_speed);

	memset(can_sessions, 0, sizeof(struct can_session_t) * ARRAY_SIZE(can_sessions));

	// init tcp
	pcb = tcp_new();
	tcp_bind(pcb, IP_ADDR_ANY, 5524);
	pcb = tcp_listen(pcb);

	tcp_accept(pcb, can_tcp_accept);
}
