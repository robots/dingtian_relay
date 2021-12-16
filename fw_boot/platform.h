#ifndef PLATFORM_H_
#define PLATFORM_H_

#define __PROGRAM_START myprogram
#include <stdint.h>
#include "stm32.h"
#include "stm32_eth.h"

//#define USE_IAP_TFTP   /* enable IAP using TFTP */
#define USE_IAP_HTTP   /* enable IAP using HTTP */

#define LIKELY(x)          __builtin_expect(!!(x), 1)
#define UNLIKELY(x)        __builtin_expect(!!(x), 0)
#define ARRAY_SIZE(x)      ((sizeof(x)/sizeof(x[0])))
#define BV(x)              (1 << (x))
#define ABS(x)             (((x)>0)?(x):(-(x)))
#define MAX(x,y)           ((x)>(y)?(x):(y))
#define MIN(x,y)           ((x)>(y)?(y):(x))

#define PACK               __attribute__ ((packed))

#define assert_param(x)
#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)

/* These can't be used after statements in c89. */
#ifdef __COUNTER__
	#define STATIC_ASSERT(e,m) \
		{ enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(!!(e)) }; }
#else
	/* This can't be used twice on the same line so ensure if using in headers
	 *    * that the headers are not included twice (by wrapping in #ifndef...#endif)
	 *       * Note it doesn't cause an issue when used on same line of separate modules
	 *          * compiled with gcc -combine -fwhole-program.  */
	#define STATIC_ASSERT(e,m) \
		{ enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(!!(e)) }; }
#endif

extern const uint32_t SystemFrequency;
extern const uint32_t SystemFrequency_SysClk;
extern const uint32_t SystemFrequency_AHBClk;
extern const uint32_t SystemFrequency_APB1Clk;
extern const uint32_t SystemFrequency_APB2Clk;

#ifdef dingtian_relay

#define HAVE_LED 1
#define HAVE_BUTTON 1
#define HAVE_MAGIC 1

#define XTEA_KEY {0x12435678, 0x12435678, 0x12435678, 0x12435678 }

#define USER_FLASH_INFOBLOCK_ADDRESS  0x0800F800
#define USER_FLASH_FIRST_PAGE_ADDRESS 0x08010000
#define USER_FLASH_LAST_PAGE_ADDRESS  0x0803F000
#define USER_FLASH_END_ADDRESS        0x0803F7FF

#define BOARD_ID 0

// magic
#define MAGIC_ADDR 0x20002000
#define MAGIC_KEY  0xDEADBEEF

// button
#define BUTTON_PIN           GPIO_Pin_0
#define BUTTON_GPIO          GPIOC
#define BUTTON_EXPECT        0
#define BUTTON_PULL          GPIO_MODE_IPU
#define RCC_EARLY_APB2       RCC_APB2Periph_GPIOC

#endif



#endif

