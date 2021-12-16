#ifndef CONSOLE_TELNET_h_
#define CONSOLE_TELNET_h_

#define CONSOLE_TELNET_FLAG_CLOSING     0x0001
#define CONSOLE_TELNET_FLAG_TIMEOUT     0x0002

void console_telnet_restart();
void console_telnet_init();

#endif
