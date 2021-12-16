/*
 * bxCAN driver for STM32 family processors
 *
 * 2010 Michal Demin
 *
 */

#include "platform.h"
#include <string.h>

#include "fifo.h"
#include "ee_config.h"
#include <stdlib.h>
#include "console.h"

#include "bsp_can.h"
#include "bxcan.h"

static struct console_command_t bxcan_cmd;
struct fifo_t *bxcan_rx_fifo;

const struct can_timing_t *bxcan_timing;
static const struct can_timing_t bxcan_timings[] = {
	{"1M",   0x01, 0x01e }, // 1Mbit CAN speed @ 72mHz internal clock
	{"800k", 0x02, 0x01b },
	{"500k", 0x03, 0x01e },
	{"250k", 0x07, 0x01e },
	{"125k", 0x0f, 0x01e },
	{"100k", 0x13, 0x01e },
	{"83k",  0x17, 0x01e },
	{"50k",  0x27, 0x01e },
	{"20k",  0x63, 0x01e },
	{"10k",  0xc7, 0x01e },
};

#define FILTER_NUM	14
struct can_filter_t bxcan_filters[FILTER_NUM] = {
	{
		.active = 1, .id = 0, .fifo = 0, .mode = CAN_FILTER_IDMASK32,
		.idmask32 = {
			.id = {
				.stid = 0, .exid = 0, .ide = 0, .rtr = 0,
			},
			.mask = {
				.stid = 0, .exid = 0, .ide = 0, .rtr = 0,
			},
		},
	},
};


uint8_t bxcan_set_baudrate(uint8_t baud)
{
	if (baud >= ARRAY_SIZE(bxcan_timings)) {
		baud = 0;
	}

	bxcan_timing = &bxcan_timings[baud];
	bxcan_hw_reinit();

	console_printf(CON_INFO, "bxcan: baudrate is %s\n", bxcan_timing->baud);

	return 0;
}

void bxcan_set_rx_fifo(struct fifo_t *fifo)
{
	bxcan_rx_fifo = fifo;
}

void bxcan_init()
{
	RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

	AFIO->MAPR |= can_gpio_remap << 13;
	gpio_init(can_gpio, can_gpio_cnt);

	NVIC_SetPriority(CAN1_TX_IRQn, 1);
	NVIC_SetPriority(CAN1_RX0_IRQn, 1);
	NVIC_SetPriority(CAN1_SCE_IRQn, 1);

	bxcan_set_baudrate(ee_config.can_speed);

	console_add_command(&bxcan_cmd);
}

void bxcan_hw_reinit(void)
{
	NVIC_DisableIRQ(CAN1_TX_IRQn);
	NVIC_DisableIRQ(CAN1_RX0_IRQn);
	NVIC_DisableIRQ(CAN1_SCE_IRQn);

	// Reset CAN1 - clears the error state
	RCC->APB1RSTR |= RCC_APB1RSTR_CAN1RST;
	RCC->APB1RSTR &= ~RCC_APB1RSTR_CAN1RST;

	// enable intertrupts
	//CAN1->IER = 0x00008F13; // enable all interrupts (except FIFOx full/overrun, sleep/wakeup)
	CAN1->IER = CAN_IER_TMEIE | CAN_IER_FMPIE0;
//	CAN1->IER |= CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE | CAN_IER_LECIE;
//	CAN1->IER |= CAN_IER_ERRIE; // FIXME

	// enter the init mode
	CAN1->MCR &= ~CAN_MCR_SLEEP;
	CAN1->MCR |= CAN_MCR_INRQ;

	// wait for it !
	while ((CAN1->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

	bxcan_filter_apply();

	/* setup timing */
	CAN1->BTR = (bxcan_timing->ts << 16) | bxcan_timing->brp;

	/* finish bxCAN setup */
	CAN1->MCR |= CAN_MCR_ABOM;
	CAN1->MCR |= CAN_MCR_TXFP | CAN_MCR_RFLM | CAN_MCR_AWUM; // automatic wakeup, tx round-robin mode
	CAN1->MCR &= ~(CAN_MCR_SLEEP | 0x10000); // we don't support sleep, no debug-freeze
	CAN1->MCR &= ~CAN_MCR_INRQ; // leave init mode

	console_printf(CON_INFO, "bxCan: init ok\n");


	NVIC_EnableIRQ(CAN1_TX_IRQn);
	NVIC_EnableIRQ(CAN1_RX0_IRQn);
//	NVIC_EnableIRQ(CAN1_SCE_IRQn);

}

void bxcan_filter_clear(void)
{
	int i;
	// setup can filters
	for (i = 0; i < FILTER_NUM; i++) {
		bxcan_filters[i].active = 0;
	}
}

void bxcan_filter_apply(void)
{
	int i;

  CAN1->FMR |= CAN_FMR_FINIT;

	// setup can filters
	for (i = 0; i < FILTER_NUM; i++) {
		uint32_t fmask = 1 << i;

		CAN1->FA1R &= ~fmask;

		if (bxcan_filters[i].active == 0)
			break;

		switch (bxcan_filters[i].mode) {
		case CAN_FILTER_IDMASK32:
			{
				CAN1->FS1R |= fmask;
				CAN1->FM1R &= ~fmask;
				break;
			}
		case CAN_FILTER_ID32:
			{
				CAN1->FS1R |= fmask;
				CAN1->FM1R |= fmask;
				break;
			}
		case CAN_FILTER_IDMASK16:
			{
				CAN1->FS1R &= ~fmask;
				CAN1->FM1R &= ~fmask;
				break;
			}
		case CAN_FILTER_ID16:
			{
				CAN1->FS1R &= ~fmask;
				CAN1->FM1R |= fmask;
				break;
			}
		}

		// set filter parameters
    CAN1->sFilterRegister[i].FR1 = bxcan_filters[i].raw[0];
    CAN1->sFilterRegister[i].FR2 = bxcan_filters[i].raw[1];

		// enable filter
		CAN1->FA1R |= fmask;
	}

  CAN1->FMR &= ~CAN_FMR_FINIT;
}

/*
void bxcan_filter_addmask(uint16_t cobid, uint16_t cobid_mask, uint8_t prio)
{
	uint8_t i = 0;

	for (i = 0; i < FILTER_NUM; i++) {
		if (bxcan_filters[i].CAN_FilterActivation == DISABLE)
			break;
	}

	// check limit
	if (i >= FILTER_NUM)
		return;

	bxcan_filters[i].CAN_FilterActivation = ENABLE;
	bxcan_filters[i].CAN_FilterNumber = i;
	bxcan_filters[i].CAN_FilterScale = CAN_FilterScale_32bit;
	bxcan_filters[i].CAN_FilterMode = CAN_FilterMode_IdMask;
	bxcan_filters[i].CAN_FilterFIFOAssignment = (prio > 0)? 1: 0;
  bxcan_filters[i].CAN_FilterIdHigh = cobid << 5;
	bxcan_filters[i].CAN_FilterIdLow = 0x0000;
	bxcan_filters[i].CAN_FilterMaskIdHigh = cobid_mask << 5;
  bxcan_filters[i].CAN_FilterMaskIdLow = 0x0004;
}
*/

/* TODO: priority scheduling */
uint8_t bxcan_send(struct can_message_t *m)
{
	uint16_t mailbox = 4;
	if (CAN1->TSR & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) {
		mailbox = (CAN1->TSR & CAN_TSR_CODE) >> 24;
	} else {
		// nothing empty ? :( this should not happen !
		// TODO: Error ?
		return 0xFF;
	}

	// clear message
	CAN1->sTxMailBox[mailbox].TIR &= CAN_TI0R_TXRQ;

	uint32_t tir = 0;

	// add RTR field
	if (m->flags & CAN_MSG_RTR) {
		tir |= 0x02;
	}

	// add msg ID
	if (m->flags & CAN_MSG_EID) {
		tir |= (m->id & 0x1fffffff) << 3;
		tir |= 0x04;
	} else {
		tir |= (m->id & 0x7ff) << 21;
	}
	CAN1->sTxMailBox[mailbox].TIR = tir;

	// setup the DLC field (lenght)
	CAN1->sTxMailBox[mailbox].TDTR = m->flags & CAN_MSG_SIZE;

	// Set up the data fields
	CAN1->sTxMailBox[mailbox].TDLR = (((uint32_t)m->data[3] << 24) | ((uint32_t)m->data[2] << 16) | ((uint32_t)m->data[1] << 8) | ((uint32_t)m->data[0]));
	CAN1->sTxMailBox[mailbox].TDHR = (((uint32_t)m->data[7] << 24) | ((uint32_t)m->data[6] << 16) | ((uint32_t)m->data[5] << 8) | ((uint32_t)m->data[4]));

	// mark message for transmission
	CAN1->sTxMailBox[mailbox].TIR |= CAN_TI0R_TXRQ;

	return 0;
}


/* TX interrupt */
void CAN1_TX_IRQHandler(void) {
	if (CAN1->TSR & CAN_TSR_RQCP0) {
		CAN1->TSR |= CAN_TSR_RQCP0;
	}
	if (CAN1->TSR & CAN_TSR_RQCP1) {
		CAN1->TSR |= CAN_TSR_RQCP1;
	}
	if (CAN1->TSR & CAN_TSR_RQCP2) {
/*		CANController_Status |= (CAN1->TSR & CAN_TSR_ALST2)?CAN_STAT_ALST:0;
		CANController_Status |= (CAN1->TSR & CAN_TSR_TERR2)?CAN_STAT_TERR:0;
		CANController_Status |= (CAN1->TSR & CAN_TSR_TXOK2)?CAN_STAT_TXOK:0;*/
		CAN1->TSR |= CAN_TSR_RQCP2;
	}

}

uint8_t bxcan_rx_ne(void)
{
	if (CAN1->RF1R & 0x03) {
		return 1;
	} else if (CAN1->RF0R & 0x03) {
		return 1;
	} else {
		return 0;
	}
}

/* RX0 fifo interrupt
 * We cannot eat the whole FIFO, instead we let NVIC process higher prio
 * interrupt and return here later.
 */
void CAN1_RX0_IRQHandler(void)
{
	bxcan_recv();
}

void bxcan_recv(void)
{
	struct can_message_t *m;

	m = fifo_get_write_addr(bxcan_rx_fifo);

	uint8_t fifo = 0;
	if (CAN1->RF1R & 0x03) {
		fifo = 1;
	} else if (CAN1->RF0R & 0x03) {
		fifo = 0;
	} else {
		return;
	}

	m->flags = 0;

	uint32_t rir = CAN1->sFIFOMailBox[fifo].RIR; 
	m->flags |= (rir & 0x02) ? CAN_MSG_RTR : 0;
	m->flags |= (rir & 0x04) ? CAN_MSG_EID : 0;

	uint32_t rdtr = CAN1->sFIFOMailBox[fifo].RDTR; 
	m->flags |= rdtr & CAN_MSG_SIZE;
	m->time = rdtr >> 16;

	if (rir & 0x04) {
		// extended id
		m->id = (uint32_t)0x1FFFFFFF & (CAN1->sFIFOMailBox[0].RIR >> 3);
	} else {
		// standard id
		m->id = (uint32_t)0x000007FF & (CAN1->sFIFOMailBox[0].RIR >> 21);
	}

	// copy data bytes
	uint32_t d;

	d = CAN1->sFIFOMailBox[fifo].RDLR;
	m->data[0] = (uint8_t)(d);
	m->data[1] = (uint8_t)(d >> 8);
	m->data[2] = (uint8_t)(d >> 16);
	m->data[3] = (uint8_t)(d >> 24);
	d = CAN1->sFIFOMailBox[fifo].RDHR;
	m->data[4] = (uint8_t)(d);
	m->data[5] = (uint8_t)(d >> 8);
	m->data[6] = (uint8_t)(d >> 16);
	m->data[7] = (uint8_t)(d >> 24);

	CAN1->RF0R = CAN_RF0R_RFOM0;

	fifo_write_done(bxcan_rx_fifo);
#if DEBUG
	console_printf(CON_INFO, "bxCan: rx done\n");
#endif
	//return fifo+1;
}

/* status change and error
 * This ISR is called periodicaly while error condition persists !
 *
 * FIXME: This is very much broken (HW side) !!!
 */
uint32_t CAN_Error = 0;

void CAN1_SCE_IRQHandler(void) {
	static uint32_t _CAN_Error = 0;
	static uint32_t _CAN_Error_Last = 0;
	static uint16_t count = 0;

	if (CAN1->ESR & (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF)) {
		console_printf(CON_ERR, "bxCan: err %08x\n", CAN1->ESR);
		// if error happened, copy the state
		_CAN_Error = CAN1->ESR;

		// abort msg transmission on Bus-Off
		if (CAN1->ESR & CAN_ESR_BOFF) {
			CAN1->TSR |= (CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2);
		}
		// clean flag - not working at all :(
		CAN1->ESR &= ~ (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF);

		// clear last error code
		CAN1->ESR |= CAN_ESR_LEC;

		// clear interrupt flag
		CAN1->MSR &= ~CAN_MSR_ERRI;

		// work around the bug in HW
		// notify only on "new" error, otherwise reset can controller
		if (_CAN_Error ^ _CAN_Error_Last) {
			count = 0;
			// notify CAN stack - emcy event
			CAN_Error = 1;
		} else {
			count ++;
			if (count > 10) {
				count = 0;
				bxcan_hw_reinit();
			}
		}
		_CAN_Error_Last = _CAN_Error;
	}
}

#define CAN_MSGS_CNT 5
static uint8_t bxcan_cmd_handler(struct console_session_t *cs, char **args)
{
//	static struct can_message_t can_msgs[CAN_MSGS_CNT];
	char out[CONSOLE_CMD_OUTBUF];
	uint8_t ret = 1;
	uint32_t len;
	//uint32_t i;
	//uint8_t val;

	if (args[0] == NULL) {
		len = tfp_sprintf(out, "no cmd\n");
		console_session_output(cs, out, len);
		return 1;
	}

	if (strcmp(args[0], "state") == 0) {
		uint8_t esr = CAN1->ESR;
		len = tfp_sprintf(out, "ESR = %08x rec=%d tec=%d lec=%d boff=%d epvf=%d ewgf=%d\n",
			esr, (esr >> 23), (esr >> 16) & 0xff, (esr >> 4) & 7, esr & 4, esr & 2, esr & 1);
		len += tfp_sprintf(&out[len], "MSR = %08x\n", CAN1->MSR);
		len += tfp_sprintf(&out[len], "BTR = %08x\n", CAN1->BTR);
		console_session_output(cs, out, len);
	} else if (strcmp(args[0], "baud") == 0) {
		int n = strtoul(args[1], NULL, 0);

		bxcan_set_baudrate(n);
	}

	return ret;
}

static struct console_command_t bxcan_cmd = {
	"can",
	bxcan_cmd_handler,
	"can menu",
	"can menu help",
	NULL
};
