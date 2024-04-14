/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <inttypes.h>

#include "board.h"
#include "usr_uart.h"

#include <hw/wb_uart.h>
#include <inttypes.h>
#include <wrc.h>
#include <dev/w1.h>
#include <ppsi/ppsi.h>
#include <wrpc.h>
#include <wr-api.h>
#include <dev/minic.h>
#include <softpll_ng.h>
#include <dev/syscon.h>
#include <dev/pps_gen.h>
#include <dev/onewire.h>
#include <temperature.h>
#include "wrc_ptp.h"
#include "hal_exports.h"
#include "lib/ipv4.h"
#include "shell.h"
#include "revision.h"

#define CALC_BAUD(baudrate) \
    ( ((( (unsigned long long)baudrate * 8ULL) << (16 - 7)) + \
      (CPU_CLOCK >> 8)) / (CPU_CLOCK >> 7) )
#define USR_UART_BAUDRATE 115200ULL

volatile struct UART_WB *usr_uart;
#define USR_UART_FRAME_HEAD 0
#define USR_UART_FRAME_BODY 1
#define USR_UART_FRAME_REPORT 2

#define USR_UART_BYTE_HEAD 0x7B
#define USR_UART_BYTE_TAIL 0x7D

static int usr_uart_state = USR_UART_FRAME_HEAD;
static int prev_key = 0;
extern struct pp_instance ppi_static[wr_num_ports];

void usr_uart_init_hw()
{
	usr_uart = (volatile struct UART_WB *)BASE_USR_UART;
	usr_uart->BCR = CALC_BAUD(USR_UART_BAUDRATE);
}

void usr_uart_write_byte(int b)
{
	while (usr_uart->SR & UART_SR_TX_BUSY);
	usr_uart->TDR = b;
}

int usr_uart_write_string(const char *s)
{
	const char *t = s;
	while (*s)
		usr_uart_write_byte(*(s++));
	return s - t;
}

static int usr_uart_poll(void)
{
	return usr_uart->SR & UART_SR_RX_RDY;
}

int usr_uart_read_byte(void)
{
	if (!usr_uart_poll())
		return -1;

	return (usr_uart->RDR & 0xff);
}

void usr_uart_print64(uint64_t num)
{
	int i;
	for(i=0;i<8;i++){
		usr_uart_write_byte((num)&0xFF);
		num = ((num)>>8);
	}
}

void usr_uart_print32(uint32_t num)
{
	int i;
	for(i=0;i<4;i++){
		usr_uart_write_byte((num)&0xFF);
		num = ((num)>>8);
	}
}

int usr_uart_cmd_report(void)
{
	uint64_t sec;
	uint32_t nsec;
	int32_t temp;
	int port;
	struct hal_port_state state;
	uint8_t link;
	uint8_t locked;
	struct pp_instance *ppi[wr_num_ports];
	uint8_t ptpmode;
	uint8_t ptp_state;
	struct wr_servo_state *s[wr_num_ports];
	uint8_t servo_state;
	int64_t delaymm;
	int64_t delayms;
	int32_t delaytxm;
	int32_t delayrxm;
	int32_t delaytxs;
	int32_t delayrxs;
	int32_t skew;
	int32_t offset;
	uint32_t update_count;

	// 0-1 head 0x7B 0x7B
	usr_uart_write_byte(USR_UART_BYTE_HEAD);
	usr_uart_write_byte(USR_UART_BYTE_HEAD);

	// 2 body 0x03
	usr_uart_write_byte(0x03); // response report status

	// 3-14 time 8byte sec, 4byte nsec
	shw_pps_gen_get_time(&sec, &nsec);
	usr_uart_print64(sec);
	usr_uart_print32(nsec);

	// 15-16 temperature
	temp = wrc_temp_get("pcb");
	temp = (temp >> 16);
	usr_uart_write_byte(temp&0xFF);
	temp = (temp >> 8);
	usr_uart_write_byte(temp&0xFF);

	for (port = 0; port < wr_num_ports; ++port) {
		wrpc_get_port_state(&state, port);
		link = (uint8_t)state.state;
		locked = (uint8_t)state.locked;
		ptpmode = (uint8_t)ptp_mode[port];

		ppi[port] = &(ppi_static[port]);
		s[port] = &((struct wr_data *)ppi[port]->ext_data)->servo_state;

		ptp_state   = (uint8_t)ppi[port]->state;
		if(ptp_state==PPS_SLAVE)
		{
			servo_state  = (uint8_t)s[port]->state;
			delaymm      = s[port]->picos_mu;
			delayms      = s[port]->delta_ms;
			delaytxm     = s[port]->delta_tx_m;
			delayrxm     = s[port]->delta_rx_m;
			delaytxs     = s[port]->delta_tx_s;
			delayrxs     = s[port]->delta_rx_s;
			skew         = s[port]->skew;
			offset       = s[port]->offset;
			update_count = s[port]->update_count;
		}
		else 
		{
			servo_state  = 0;
			delaymm      = 0;
			delayms      = 0;
			delaytxm     = 0;
			delayrxm     = 0;
			delaytxs     = 0;
			delayrxs     = 0;
			skew         = 0;
			offset       = 0;
			update_count = 0;
		}
		// 17 / 66
		usr_uart_write_byte(link); // 0: link down, 1 link up
		// 18 / 67
		usr_uart_write_byte(locked); // 0: unlocked, 1: locked
		// 19 / 68
		// ptpmode :	0: UNKNOWN, 1: GM, 2: MASTER, 3: SLAVE
		usr_uart_write_byte(ptpmode); 
		// 20 / 69
		// ptp state:	0: none,		1: PPS_INITIALIZING,2: PPS_FAULTY,
		//				3: PPS_DISABLED,4: PPS_LISTENING,	5: PPS_PRE_MASTER,
		//				6: PPS_MASTER, 	7: PPS_PASSIVE,		8: PPS_UNCALIBRATED,
		//				9: PPS_SLAVE,	100: WRS_PRESENT,	101: WRS_S_LOCK,
		//				102: WRS_M_LOCK,103: WRS_LOCKED,	104, 108-116:WRS_CALIBRATION,
		//				105: WRS_CALIBRATED, 106: WRS_RESP_CALIB_REQ, 107: WRS_WR_LINK_ON,
		usr_uart_write_byte(ptp_state);
		// 21 / 70
		// servo_state	0: WR_UNINITIALIZED,	1: WR_SYNC_NSEC,	2: WR_SYNC_TAI,
		// 				3: WR_SYNC_PHASE,		4: WR_TRACK_PHASE,	5: WR_WAIT_OFFSET_STABLE,
		usr_uart_write_byte(servo_state);
		// 22-29 / 71-78 round trip delay, ps
		usr_uart_print64(delaymm);
		// 30-37 / 79-86 master to slave delay, ps
		usr_uart_print64(delayms); 
		// 38-41 / 87-90 fixed delay txm, ps
		usr_uart_print32(delaytxm);
		// 42-45 / 91-94 fixed delay txs, ps
		usr_uart_print32(delaytxs);
		// 46-49 / 95-98 fixed delay rxm, ps
		usr_uart_print32(delayrxm);
		// 50-53 / 99-102 fixed delay rxs, ps
		usr_uart_print32(delayrxs);
		// 54-57 / 103-106 skew(delayms - prev_delayms), ps
		usr_uart_print32(skew);
		// 58-61 / 107-110 calculated clock offset, ps
		usr_uart_print32(offset);
		// 62-65 / 111-114 servo update count
		usr_uart_print32(update_count);
	}

	// tail 0x7D 0x7D 115-116
	usr_uart_write_byte(USR_UART_BYTE_TAIL);
	usr_uart_write_byte(USR_UART_BYTE_TAIL);

	return 1;
}

int usr_uart_response(void)
{
	int c;
	c = usr_uart_read_byte();

	if (c < 0)
		return 0;

	switch(usr_uart_state)
	{
		case USR_UART_FRAME_HEAD: // USR_UART_BYTE_HEAD & USR_UART_BYTE_HEAD
			if ((c==USR_UART_BYTE_HEAD) && (prev_key==USR_UART_BYTE_HEAD)) {
				usr_uart_state = USR_UART_FRAME_BODY;
			}
			break;
		case USR_UART_FRAME_BODY: 
			if(c==0x02) // 0x02 : report status
				usr_uart_state = USR_UART_FRAME_REPORT;
			else
				usr_uart_state = USR_UART_FRAME_HEAD;
			break;
		case USR_UART_FRAME_REPORT: // USR_UART_BYTE_TAIL & USR_UART_BYTE_TAIL
			if((c==USR_UART_BYTE_TAIL) && (prev_key==USR_UART_BYTE_TAIL))
			{
				usr_uart_cmd_report();
				usr_uart_state = USR_UART_FRAME_HEAD;
			}
			break;
		default:
			break;
	}
	prev_key = c;
	return 1;
}

DEFINE_WRC_TASK(usruart) = {
	.name = "usr_uart",
	.job = usr_uart_response,
};