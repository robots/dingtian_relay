#ifndef RTC_F1_h_
#define RTC_F1_h_

#include "platform.h"

void rtc_f1_init();
uint32_t rtc_f1_get_time(void);
void rtc_f1_set_time(uint32_t t);

#endif
