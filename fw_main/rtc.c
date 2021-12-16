
#include "platform.h"
#include "config.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "printf.h"
#include "console.h"
#include "rtc.h"
#ifdef STM32F1
#include "rtc_f1.h"
#endif
#ifdef STM32F4
#include "rtc_f4.h"
#endif

static struct console_command_t rtc_console_cmd;

void rtc_init(void)
{
#ifdef STM32F1
	rtc_f1_init();
#endif
#ifdef STM32F4
	rtc_f4_init();
#endif
	console_add_command(&rtc_console_cmd);
}

time_t rtc_get_time(void)
{
	time_t t = 0;

#ifdef STM32F1
	t = (time_t)rtc_f1_get_time();
#endif
#ifdef STM32F4
	t = (time_t)rtc_f4_get_time();
#endif
	
	return t;
}

void rtc_set_time(time_t t)
{
#ifdef STM32F1
	rtc_f1_set_time(t);
#endif
#ifdef STM32F4
	rtc_f4_set_time(t);
#endif

}

static uint8_t rtc_console_handler(struct console_session_t *cs, char **args)
{
	char out[CONSOLE_CMD_OUTBUF];
	uint8_t ret = 1;
	uint32_t len;

	if ((args[0] == NULL) || (strcmp(args[0], "date") == 0)) {
		time_t time = rtc_get_time();
		struct tm * timeinfo;
		timeinfo = localtime(&time);
		len = tfp_sprintf(out, "Time is %02d.%02d.%04d %02d:%02d:%02d\n",
			timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		console_session_output(cs, out, len);
	}	else if (strcmp(args[0], "show") == 0) {
		len = tfp_sprintf(out, "Time is %d\n", rtc_get_time());
		console_session_output(cs, out, len);
	} else if (strcmp(args[0], "set") == 0) {
		rtc_set_time(strtoll(args[1], NULL, 0));
	}

	return ret;
}

static struct console_command_t rtc_console_cmd = {
	"rtc",
	rtc_console_handler,
	"Real time clock menu",
	"Real time clock\n show - shows curent time in seconds\n set x - sets current time to value x in unix time\n date - shows current time as date/time",
	NULL
};
