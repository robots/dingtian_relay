#include "platform.h"

#include "wdg.h"

#define IWDG_KEY_RELOAD                  ((uint16_t)0xAAAA)
#define IWDG_KEY_ENABLE                  ((uint16_t)0xCCCC)
#define IWDG_KEY_WRITE_ACCESS_ENABLE     ((uint16_t)0x5555)
#define IWDG_KEY_WRITE_ACCESS_DISABLE    ((uint16_t)0x0000)

void wdg_enable(void)
{
	IWDG->KR = IWDG_KEY_ENABLE;

	// 40khz/32 = 1.25khz
	IWDG->KR = IWDG_KEY_WRITE_ACCESS_ENABLE;
	IWDG->PR = IWDG_PR_PR_0 | IWDG_PR_PR_1;
	// 0xfff / 1024hz = 4s
	IWDG->RLR = 0xfff;

	uint32_t timeout = 0;
	while(IWDG->SR) {
		timeout ++;
		if (timeout == 0xffffff) {
			return;
		}
	}

	IWDG->KR = IWDG_KEY_RELOAD;

	return;
}

void wdg_feed(void)
{
	IWDG->KR = IWDG_KEY_RELOAD;
}

