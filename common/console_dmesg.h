#ifndef CONSOLE_DMESG_h_
#define CONSOLE_DMESG_h_

#include "platform.h"

extern struct fifo_t console_dmesg_fifo;

void console_dmesg_init();
void console_dmesg_putbuf(const char *buf, uint32_t len);

#endif
