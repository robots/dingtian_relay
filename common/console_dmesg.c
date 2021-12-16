#include "platform.h"

#include <string.h>

#include "config.h"
#include "fifo.h"

#include "console.h"
#include "console_dmesg.h"

#define DMESG_INIT_MAGIC 0xDEADDEAD

static struct console_command_t console_dmesg_cmd;

struct console_dmesg_t {
	uint32_t magic;
	struct fifo_t fifo;
	uint8_t buf[CONSOLE_DMESG_BUFFER];
};

struct console_dmesg_t __attribute__((section(".bkpram"))) console_dmesg;

void console_dmesg_init()
{
	if (console_dmesg.magic != DMESG_INIT_MAGIC) {
		console_dmesg.magic = DMESG_INIT_MAGIC;
		fifo_init(&console_dmesg.fifo, console_dmesg.buf, sizeof(uint8_t), CONSOLE_DMESG_BUFFER);
	}

	console_add_command(&console_dmesg_cmd);
}

void console_dmesg_putbuf(const char *buf, uint32_t len)
{
	// make space in fifo, if full
	if (len > fifo_get_write_count(&console_dmesg.fifo)) {
		uint32_t diff = len - fifo_get_write_count(&console_dmesg.fifo) + 1;

		// need to lock the fifo, uart could read data
		console_lock();
		fifo_read_done_count(&console_dmesg.fifo, diff);
		console_unlock();
	}

	fifo_write_buf(&console_dmesg.fifo, buf, len);
}

static uint8_t console_dmesg_handler(struct console_session_t *cs, char **args)
{
	uint8_t ret = 1;
	uint8_t del = 0;

	if (args[0] != NULL) {
		if (strcmp(args[0], "fl") == 0) {
			del = 1;
		}
	}

	struct fifo_t *f = &console_dmesg.fifo;
	uint32_t rl;
	uint32_t totlen = 0;

	// hackhish access to fifo
	if (f->write > f->read) {
		rl = f->write - f->read;
		cs->output(cs, fifo_get_read_addr(f), rl);
		totlen += rl;
	} else if (f->read > f->write) {
		rl = f->e_num - f->read;
		cs->output(cs, fifo_get_read_addr(f), rl);
		totlen += rl;
		rl = f->write;
		cs->output(cs, (char *)f->buffer, rl);
		totlen += rl;
	}

	if (del) {
		fifo_read_done_count(f, totlen);
	}

	return ret;
}

static struct console_command_t console_dmesg_cmd = {
	"dmesg",
	console_dmesg_handler,
	"Prints message buffer",
	"Prints message buffer\nArguments:\n fl - print and flush the message buffer",
	NULL
};

