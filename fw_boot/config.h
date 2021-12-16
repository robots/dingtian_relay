#ifndef CONFIG_h_
#define CONFIG_h_

#define EE_USERNAME_LEN     48
#define EE_PASSWORD_LEN     48

#define SYSTIME_TIMERS      20

#define CONSOLE_DISABLE     1
#define CONSOLE_CMD_ARGS    10
#define CONSOLE_CMD_OUTBUF  500

#define CONSOLE_TELNET_TIMEOUT (5*60)
#define CONSOLE_SESSION_NUM    3
#define CONSOLE_UART_BUFFER    (1*1024)

#define CONSOLE_DMESG_BUFFER   (1*1024)
#define CONSOLE_CMD_BUFFER     100

#define HAVE_RTC            1
#define HAVE_RTC_XTAL       0
#define HAVE_SNTP           1
#define HAVE_CAN            1

#define SNTP_STARTUP_DELAY  1
#define SNTP_STARTUP_DELAY_TIME     (random_get() & 0xFFFF)
#define SNTP_CHECK_RESPONSE 1


#endif
