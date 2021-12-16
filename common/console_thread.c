#include "platform.h"

#include "console.h"

#include "console_thread.h"


void console_thread_init(void)
{

}

void console_thread_handle(struct console_session_t *cs)
{
	if (cs->ct) {
		if (PT_SCHEDULE(cs->ct(cs)) == 0) {
			// task ended
			cs->ct = NULL;
		}
	}
}

int console_thread_start(struct console_session_t *cs, console_thread_t ct)
{
	if (cs->ct) {
		return 1;
	}

	PT_INIT(&cs->pt);
	cs->ct = ct;

	return 0;
}


