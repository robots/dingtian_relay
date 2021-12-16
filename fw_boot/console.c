#include "platform.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "config.h"
#include "ee_config.h"
#include "fifo.h"
#include "netconf.h"
#include "utils.h"

#include "printf.h"
#include "console.h"


void console_init(void)
{
}

void console_add_command(struct console_command_t *new_cmd)
{
	(void)new_cmd;
}

void console_lock(void)
{
}

void console_unlock(void)
{
}

void cprintf(char *fmt, ...)
{
	(void)fmt;
}

void console_printf(uint8_t level, const char *fmt, ...)
{
	(void)level;
	(void)fmt;
}

void console_write(uint8_t level, const char *buf, uint32_t len)
{
	(void)level;
	(void)buf;
	(void)len;
}

void console_session_output(struct console_session_t * cs, const char *buf, uint32_t len)
{
	(void)cs;
	(void)buf;
	(void)len;
}

void console_session_printf(struct console_session_t *cs, const char *fmt, ...)
{
	(void)cs;
	(void)fmt;
}

uint8_t console_session_init(struct console_session_t **cs, session_output_t output, session_close_t close)
{
	(void)cs;
	(void)output;
	(void)close;

	return 0;
}

void console_session_close(struct console_session_t *cs)
{
	(void)cs;
}

void console_cmd_parse(struct console_session_t *cs, char *buf, uint32_t len)
{
	(void)cs;
	(void)buf;
	(void)len;
}

