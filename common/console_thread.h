#ifndef CONSOLE_THREAD_h_
#define CONSOLE_THREAD_h_

#include "console.h"

void console_thread_init(void);
void console_thread_handle(struct console_session_t *cs);
int console_thread_start(struct console_session_t *cs, console_thread_t ct);

#endif
