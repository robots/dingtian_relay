#include "platform.h"

#include <string.h>

#include "gpio.h"
#include "fifo.h"
#include "ee_config.h"

#include "bsp_console_uart.h"

#include "console.h"
#include "bsp_console_uart.h"
#include "console_uart.h"

const uint32_t console_uart_baudrates[] = {
	115200,
	230400,
	921600,
};

struct fifo_t console_uart_fifo;
uint8_t __attribute__((section(".lwipram"))) console_uart_buf[CONSOLE_UART_BUFFER];

struct console_session_t *console_uart_session;

// dma stuff
volatile uint8_t console_uart_sending = 0;
volatile uint32_t console_uart_sending_size;

static void console_uart_send(struct console_session_t *cs, const char *buf, uint32_t len);

static void console_uart_hw_init(uint32_t speed)
{
	USART1->CR1 &= ~USART_CR1_UE;

	USART1->CR1 &= ~USART_CR1_M;
	
	USART1->CR2 &= ~(3<<12);
	USART1->CR2 |= 0 << 12;

	USART1->CR2 &= ~(3<<9);

#if 0
	uint32_t i = (0x19 * SystemFrequency_APB1Clk) / (0x04 * br);
	uint32_t brr = (i/0x64) << 4;
	uint32_t f = i - (0x64 * i);
	brr |= ((((f * 0x10) + 0x32) / 0x64)) & 0x0f;

	USART1->BRR = brr;
#else
	USART1->BRR = ((2*SystemFrequency_APB1Clk) / speed + 1) / 2;
#endif

	USART1->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void console_uart_init(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;

	gpio_init(console_uart_gpio, console_uart_gpio_cnt);

	/* DMA_Channel (triggered by USART Tx event) */
	DMA1_Channel4->CPAR = (uint32_t)&USART1->DR;
	DMA1_Channel4->CCR = 0x3092;

	NVIC_SetPriority(USART1_IRQn, 1);
	NVIC_EnableIRQ(USART1_IRQn);

	NVIC_SetPriority(DMA1_Channel4_IRQn, 1);
	NVIC_EnableIRQ(DMA1_Channel4_IRQn);


	USART1->CR1 |= USART_CR1_RXNEIE;
	USART1->CR3 |= USART_CR3_DMAT;

	if (ee_config.uart_speed > ARRAY_SIZE(console_uart_baudrates)) {
		ee_config.uart_speed = 0;
	}

	console_uart_hw_init(console_uart_baudrates[ee_config.uart_speed]);


	fifo_init(&console_uart_fifo, console_uart_buf, sizeof(uint8_t), CONSOLE_UART_BUFFER);


	if (console_session_init(&console_uart_session, console_uart_send, NULL)) {
		// this should not happen, as uart is first session
		char *err = "Unable to init session for uart\n";
		console_uart_send(NULL, err, strlen(err));

		return;
	}

	console_uart_session->flags |= CONSOLE_FLAG_ECHO;
	console_uart_session->auth_state = CON_AUTH_OK;
	console_uart_session->verbosity = ee_config.uart_verbosity;
}

void console_uart_disable_int(void)
{
	NVIC_DisableIRQ(USART1_IRQn);
}

void console_uart_enable_int(void)
{
	NVIC_EnableIRQ(USART1_IRQn);
}

void console_uart_apply(void)
{
	if (ee_config.uart_speed > ARRAY_SIZE(console_uart_baudrates)) {
		ee_config.uart_speed = 0;
	}

	console_uart_hw_init(console_uart_baudrates[ee_config.uart_speed]);

	console_uart_session->verbosity = ee_config.uart_verbosity;
}

/*
 * This function will enqueue buffer into the outgoing fifo. If fifo
 * overruns, data is dropped. (in this case use faster uart speed!)
 */
static void console_uart_send(struct console_session_t *cs, const char *buf, uint32_t len)
{
	(void)cs;

	fifo_write_buf(&console_uart_fifo, buf, len);

	NVIC_DisableIRQ(DMA1_Channel4_IRQn);

	if (UNLIKELY(console_uart_sending == 0)) {
		console_uart_sending = 1;
		console_uart_sending_size = (uint32_t)fifo_get_read_count_cont(&console_uart_fifo);
		DMA1_Channel4->CMAR = (uint32_t)fifo_get_read_addr(&console_uart_fifo);
		DMA1_Channel4->CNDTR = console_uart_sending_size;
		DMA1_Channel4->CCR |= 1;
	}

	NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

void DMA1_Channel4_IRQHandler()
{
	DMA1_Channel4->CCR &= ~1;
	DMA1->IFCR |= DMA_IFCR_CTCIF4;

	fifo_read_done_count(&console_uart_fifo, console_uart_sending_size);

	if (UNLIKELY(FIFO_EMPTY(&console_uart_fifo))) {
		console_uart_sending = 0;
		console_uart_sending_size = 0;
		return;
	}

	console_uart_sending_size = (uint32_t)fifo_get_read_count_cont(&console_uart_fifo);
	DMA1_Channel4->CMAR = (uint32_t)fifo_get_read_addr(&console_uart_fifo);
	DMA1_Channel4->CNDTR = console_uart_sending_size;
	DMA1_Channel4->CCR |= 1;
}

void USART1_IRQHandler()
{
	char ch;

	if (USART1->SR & USART_SR_RXNE) {
		ch = USART1->DR & 0xff;

		if (console_uart_session != NULL) {
			console_cmd_parse(console_uart_session, &ch, 1);
		}
	}
}

