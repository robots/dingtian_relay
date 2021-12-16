#include "platform.h"
#include "ee_config.h"
#include "console.h"

#include "lwip/opt.h"
#include "disc.h"
#include "netconf.h"
#include "systime.h"

#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

#define DISCOVERY_REQ "DINGTIAN?"
#define DISCOVERY_LEN (11)

#define RSP_BUF_LEN (15)

static struct udp_pcb* disc_pcb;

static void disc_send(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port)
{
	struct pbuf *q = pbuf_alloc(PBUF_TRANSPORT, RSP_BUF_LEN, PBUF_RAM);

	if (q) {
#if ETH_BOOT
		tfp_sprintf(q->payload, "DINGTIAN:BOOT");
#else
		tfp_sprintf(q->payload, "DINGTIAN:%02x%02x", SW_VER, SW_BUILD);
#endif

		err_t err = udp_sendto(pcb, q, addr, port);
		if (err) {
			console_printf(CON_ERR, "Disc: error sending discovery %d\n", err);
		}
	} else {
		console_printf(CON_ERR, "Disc: out of memory\n");
	}

	pbuf_free(q);
}

static void disc_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	(void)arg;

	if (p != NULL) {
		if (pbuf_memcmp(p, 0, DISCOVERY_REQ, DISCOVERY_LEN) == 0) {
			console_printf(CON_INFO, "Disc: Got request from '%A'\n", addr->addr);

			disc_send(pcb, addr, port);
		}

		pbuf_free(p);
	}
}

void disc_send_gw(void)
{
	disc_send(disc_pcb, &netif.gw, disc_pcb->local_port);
}

void disc_send_bcast(void)
{
	ip_addr_t ipbcast;

	ipbcast.addr = (netif.ip_addr.addr & netif.netmask.addr) | ~netif.netmask.addr;
	disc_send(disc_pcb, &ipbcast, disc_pcb->local_port);
}

void disc_init(void)
{
	int port = ee_config.discovery_port;

	if (port == 0) {
		port = 11111;
		//return;
	}

	if (disc_pcb == NULL) {
		disc_pcb = udp_new();
		LWIP_ASSERT("Failed to allocate udp pcb for disc", disc_pcb != NULL);

		udp_bind(disc_pcb, IP_ADDR_ANY, port);
		udp_recv(disc_pcb, disc_recv, NULL);
  }
}
