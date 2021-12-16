/*
 * Michal Demin 2014
 *
 * send function relies on fact:
 * - TCP_SND_BUF = 4*1460
 * - dmesg buf is smaller (half?) than TCP_SND_BUF
 * - dmesg + other text fits into SND_BUF before it is dispatched to ethernet
 *
 * if not, local caching needs to be implemented:
 * - fifo
 * - sent callback
 * - ...
 *
 * or
 * - call tcp_output function in send function
 *   - makes stuff slower (does it?) / skips nagel algo
 */
#include "platform.h"

#include <string.h>
#include "lwip/tcp.h"

#include "config.h"
#include "systime.h"

#include "lwip/mem.h"

#include "console.h"
#include "console_telnet.h"

#define TELNET_OPT_TRANSMIT_BINARY  0
#define TELNET_OPT_ECHO             1
#define TELNET_OPT_SUPRESS_GO_AHEAD 3
#define TELNET_OPT_STATUS           5
#define TELNET_OPT_TIMING_MARK      6
#define TELNET_OPT_RCTE             7
#define TELNET_OPT_TTYPE            24
#define TELNET_OPT_NAWS             31
#define TELNET_OPT_LINEMODE         34
#define TELNET_OPT_NEWENV           39

#define TELNET_SE    240
#define TELNET_SB    250
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254
#define TELNET_IAC   255

#undef DEBUG


enum {
	TELNET_STATE_DATA,
	TELNET_STATE_IAC,
	TELNET_STATE_OPT,
};

struct console_telnet_session_t {
	struct tcp_pcb *pcb;

	uint32_t flags;
	uint32_t state;

	uint32_t last_recv;
	char last_char;
};

struct tcp_pcb *console_telnet_pcb;


static void console_telnet_send_opt(struct console_session_t *cs, uint8_t cmd, uint8_t opt)
{
	struct console_telnet_session_t *cts = cs->priv;

	uint8_t pkt[3];

	pkt[0] = TELNET_IAC;
	pkt[1] = cmd;
	pkt[2] = opt;

	tcp_write(cts->pcb, pkt, 3, TCP_WRITE_FLAG_COPY);
	tcp_output(cts->pcb);
}

const char *cmds[] = {
	"SB",
	"WILL",
	"WONT",
	"DO",
	"DONT",
};

const char *cmdtostr(int i)
{
	if (i < 250) {
		return "";
	}

	return cmds[i-250];
}

#if 0
static void console_telnet_process_option(struct console_session_t *cs, uint8_t cmd, uint8_t opt)
{
	if (opt == TELNET_OPT_ECHO) {
		if (cmd == TELNET_WILL) {
			console_telnet_send_opt(cs, TELNET_DO, TELNET_OPT_ECHO);
			cs->flags &= ~CONSOLE_FLAG_ECHO;
		} else if (cmd == TELNET_WONT) {
			console_telnet_send_opt(cs, TELNET_DONT, TELNET_OPT_ECHO);
			cs->flags |= CONSOLE_FLAG_ECHO;
		} else if (cmd == TELNET_DO) {
			console_telnet_send_opt(cs, TELNET_WILL, TELNET_OPT_ECHO);
			cs->flags |= CONSOLE_FLAG_ECHO;
		} else if (cmd == TELNET_DONT) {
			console_telnet_send_opt(cs, TELNET_WONT, TELNET_OPT_ECHO);
			cs->flags &= ~CONSOLE_FLAG_ECHO;
		}
#ifdef DEBUG
		console_printf(CON_DEBUG, "Telnet: %s echo flag: %x\n", cmdtostr(cmd), cs->flags);
#endif
	} else if (opt == TELNET_OPT_LINEMODE) {
		if (cmd == TELNET_WILL) {
			console_telnet_send_opt(cs, TELNET_DONT, opt);
		}
	} else {
		// only echo is supported
		if (cmd == TELNET_WILL || cmd  == TELNET_WONT) {
			console_telnet_send_opt(cs, TELNET_DONT, opt);
		} else if (cmd == TELNET_DO || cmd == TELNET_DONT) {
			console_telnet_send_opt(cs, TELNET_WONT, opt);
		}
#ifdef DEBUG
		console_printf(CON_DEBUG, "Telnet: opt %s %d\n", cmdtostr(cmd), opt);
#endif
	}

}
#endif

static void console_telnet_process(struct console_session_t *cs, char c)
{
	struct console_telnet_session_t *cts = cs->priv;

	if (cts->state == TELNET_STATE_DATA) {
		if (c == TELNET_IAC) {
			cts->state = TELNET_STATE_IAC;
		} else {
			console_cmd_parse(cs, &c, 1);
		}
	} else if (cts->state == TELNET_STATE_IAC) {
		cts->last_char = c;
		cts->state = TELNET_STATE_OPT;
	} else if (cts->state == TELNET_STATE_OPT) {
		//console_telnet_process_option(cs, cts->last_char, c);
		cts->state = TELNET_STATE_DATA;
	} else {
		// should not be here at all
#ifdef DEBUG
		console_printf(CON_DEBUG, "Telnet: unknown state\n");
#endif
		cts->state = TELNET_STATE_DATA;
		console_telnet_process(cs, c);
	}
}

static void console_telnet_send(struct console_session_t *cs, const char *buf, uint32_t len)
{
	struct console_telnet_session_t *cts = cs->priv;
	err_t err;

	err = tcp_write(cts->pcb, buf, len, TCP_WRITE_FLAG_COPY);
	if (err == ERR_MEM) {
		console_printf(CON_ERR, "Telnet: tcp_write no mem\n");
	} else if (err != ERR_OK) {
		console_printf(CON_ERR, "Telnet: tcp_write failed with err = %d\n", err);
	}
	tcp_output(cts->pcb);
}

static void console_telnet_close(struct tcp_pcb *pcb, struct console_session_t *cs)
{
	struct console_telnet_session_t *cts = NULL;

	if (cs) {
		cts = cs->priv;
		cts->flags |= CONSOLE_TELNET_FLAG_CLOSING;
	}

	tcp_arg(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);

	if (tcp_close(pcb) != ERR_OK) {
		// not ready to close, retry from poll
		tcp_arg(pcb, cs);
	} else {
		// closing succeeded, we are done with this session
		if (cts != NULL) {
			mem_free(cts);
		}

		if (cs != NULL) {
			cs->priv = NULL;
		}
		console_session_close(cs);

	}
}

static void console_telnet_session_close(struct console_session_t *cs)
{
	struct console_telnet_session_t *cts = NULL;

	cts = cs->priv;
	cts->flags |= CONSOLE_TELNET_FLAG_CLOSING;
}


static err_t console_telnet_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct console_session_t *cs = (struct console_session_t *)arg;
	struct console_telnet_session_t *cts = cs->priv;

	char c;
	uint32_t i;

	if (err != ERR_OK) {
		console_printf(CON_INFO, "Telnet: recv err not ok err = %d\n", err);
		console_telnet_close(pcb, cs);
		return ERR_OK;
	}

	if (err == ERR_OK && p == NULL) {
#ifdef DEBUG
		console_printf(CON_DEBUG, "Telnet: remote party quit\n", pcb->remote_ip.addr);
#endif
		console_telnet_close(pcb, cs);
		return ERR_OK;
	}

	if (err == ERR_OK && p != NULL) {

		tcp_recved(pcb, p->tot_len);

		cts->last_recv = systime_get();

		for (i = 0; i < p->tot_len; i++) {
			pbuf_copy_partial(p, &c, 1, i);
			console_telnet_process(cs, c);
		}

		pbuf_free(p);
	}

	return ERR_OK;
}

/*
static err_t console_telnet_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	struct console_session_t *cs = (struct console_session_t *)arg;
	struct console_telnet_session_t *cts = cs->priv;

	LWIP_UNUSED_ARG(len);

	if (cts->data_left > 0) {
		send_data(pcb, cs);
	} else {
		if (cts->flags & CONSOLE_TELNET_FLAG_CLOSING) {
			console_telnet_close(pcb, cs);
		}
	}

	return ERR_OK;
}
*/

static void console_telnet_err(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(err);

	struct console_session_t *cs = (struct console_session_t *)arg;
	struct console_telnet_session_t *cts = NULL;

	if (cs != NULL) {
		cts = cs->priv;
		mem_free(cts);
		cs->priv = NULL;
		console_session_close(cs);
	}

}

static err_t console_telnet_poll(void *arg, struct tcp_pcb *pcb)
{
	struct console_session_t *cs = (struct console_session_t *)arg;
	struct console_telnet_session_t *cts = NULL;

	if (cs == NULL) {
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	cts = cs->priv;
#ifdef DEBUG
	static uint32_t last, now;
	now = systime_get();
	console_printf(CON_DEBUG, "Telnet:  poll %d %d\n", now, now-last);
	last = now;
#endif

	if (cts->flags & CONSOLE_TELNET_FLAG_CLOSING) {
#ifdef DEBUG
		console_printf(CON_DEBUG, "Telnet: closing from poll\n");
#endif
		console_telnet_close(pcb, cs);
		return ERR_OK;
	}

	if (cts->flags & CONSOLE_TELNET_FLAG_TIMEOUT) {
		if (systime_get() - cts->last_recv > SYSTIME_SEC(CONSOLE_TELNET_TIMEOUT)) {
			console_printf(CON_INFO, "Telnet: timeout\n");

			console_telnet_close(pcb, cs);
		}
	}

	return ERR_OK;
}

static err_t console_telnet_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	struct console_session_t *cs = NULL;
	struct console_telnet_session_t *cts = NULL;

	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

	//tcp_setprio(pcb, TCP_PRIO_MIN);

	console_printf(CON_INFO, "Telnet: connection from %A\n", pcb->remote_ip.addr);

	if (console_session_init(&cs, console_telnet_send, console_telnet_session_close)) {
		// error, no session, drop connection.
		console_printf(CON_ERR, "Telnet: no session, dropping\n");
		// have to abort here, cannot call tcp_close, as it can fail and we have no way of saving state
		tcp_abort(pcb);
		return ERR_ABRT;
	}

	cts = mem_malloc(sizeof(struct console_telnet_session_t));
	if (cts == NULL) {
		console_printf(CON_ERR, "Telnet: memory low\n");
		// same as above
		tcp_abort(pcb);
		console_session_close(cs);
		return ERR_ABRT;
	}

	cs->priv = cts;
	cs->flags = CONSOLE_FLAG_ECHO;
	cts->pcb = pcb;
	cts->state = TELNET_STATE_DATA;
	cts->last_recv = systime_get();
	cts->last_char = 0;
	cts->flags = 0;

	tcp_arg(pcb, cs);
	tcp_recv(pcb, console_telnet_recv);
	//tcp_sent(pcb, console_telnet_sent);
	tcp_err(pcb, console_telnet_err);
	tcp_poll(pcb, console_telnet_poll, 1);

	// send options
	console_telnet_send_opt(cs, TELNET_WILL, TELNET_OPT_SUPRESS_GO_AHEAD);
	console_telnet_send_opt(cs, TELNET_WILL, TELNET_OPT_ECHO);
	console_telnet_send_opt(cs, TELNET_DO, TELNET_OPT_SUPRESS_GO_AHEAD);
	console_telnet_send_opt(cs, TELNET_DONT, TELNET_OPT_ECHO);
	console_telnet_send_opt(cs, TELNET_DONT, TELNET_OPT_LINEMODE);
	console_telnet_send_opt(cs, TELNET_WONT, TELNET_OPT_LINEMODE);
	//console_telnet_send_opt(cs, TELNET_DO, TELNET_OPT_NAWS);
	//console_telnet_send_opt(cs, TELNET_DO, TELNET_OPT_TTYPE);
	//console_telnet_send_opt(cs, TELNET_DO, TELNET_OPT_NEWENV);

	console_print_prompt(cs);

	return ERR_OK;
}

void console_telnet_restart()
{
	tcp_abort(console_telnet_pcb);
	console_telnet_pcb = NULL;
	console_telnet_init();
}

void console_telnet_init()
{
	struct tcp_pcb *pcb;

	pcb = tcp_new();
	tcp_bind(pcb, IP_ADDR_ANY, 23);
	console_telnet_pcb = tcp_listen(pcb);

	tcp_accept(console_telnet_pcb, console_telnet_accept);
}
