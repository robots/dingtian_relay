#ifndef RTC_h_
#define RTC_h_

#include "platform.h"
#include <sys/time.h>

#define SNTP_SET_SYSTEM_TIME(sec)  rtc_set_time(sec)
#define SNTP_GET_SYSTEM_TIME(sec, us)     do { (sec) = rtc_get_time(); (us) = 0; } while(0)

void rtc_init(void);
time_t rtc_get_time(void);
void rtc_set_time(time_t t);

#endif

