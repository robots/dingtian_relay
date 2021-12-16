#include "platform.h"

#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "utils.h"

static struct console_command_t ver_cmd;

void ver_init(void)
{
	console_add_command(&ver_cmd);	
}

static uint8_t ver_cmd_handler(struct console_session_t *cs, char **args)
{
	(void)args;
	char out[CONSOLE_CMD_OUTBUF];
	uint32_t len = 0;

	len = tfp_sprintf(out, "Current version/build: %02x/%02x\n", SW_VER, SW_BUILD);
	console_session_output(cs, out, len);

	return 0;
}

static struct console_command_t ver_cmd = {
	"version",
	ver_cmd_handler,
	"Version",
	"Version",
	NULL
};
