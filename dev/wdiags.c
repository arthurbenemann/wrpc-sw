/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011-2021 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */


#include "board.h"
#include "dev/wdiags.h"

#include <hw/wrc_diags_regs.h>

#include <errno.h>
#include <string.h>

#define WDIAGS_VERSION 2

int wdiag_write( uint32_t reg, uint32_t value )
{
//	pp_printf("wdiag_write %x %x\n",reg,value);
	// fixme: there's a max of 64 diag registers.
	writel( value, (void*) ( BASE_WDIAGS_PRIV + reg ) );
}

uint32_t wdiag_read( uint32_t reg )
{
	return readl( (void*) ( BASE_WDIAGS_PRIV + reg )  );
}


int wdiag_set_valid(int enable)
{
	uint32_t ctl = wdiag_read( WRC_DIAGS_CTRL );
	if (enable)
	{
		wdiag_write( WRC_DIAGS_CTRL, ctl | WRC_DIAGS_CTRL_DATA_VALID );
	}
	else
	{
		wdiag_write( WRC_DIAGS_CTRL, ctl & ~WRC_DIAGS_CTRL_DATA_VALID );
	}

	return wdiag_read( WRC_DIAGS_CTRL );
}

int wdiag_get_valid(void)
{
	uint32_t ctl = wdiag_read( WRC_DIAGS_CTRL );

	return (ctl & WRC_DIAGS_CTRL_DATA_VALID ) ? 1 : 0;
}

int wdiag_get_snapshot(void)
{
	uint32_t ctl = wdiag_read( WRC_DIAGS_CTRL );

	return (ctl & WRC_DIAGS_CTRL_DATA_SNAPSHOT ) ? 1 : 0;
}

void wdiags_write_servo_state(int wr_mode, uint8_t servostate, uint64_t mu,
			      uint64_t dms, int32_t asym, int32_t cko,
			      int32_t setp, int32_t ucnt, uint32_t restart_cnt, uint64_t up_timestamp )
{
	uint32_t sstat   = wr_mode ? WRC_DIAGS_WDIAG_SSTAT_WR_MODE:0;
	sstat  |= servostate << WRC_DIAGS_WDIAG_SSTAT_SERVOSTATE_SHIFT;

	wdiag_write( WRC_DIAGS_WDIAG_SSTAT, sstat );
	wdiag_write( WRC_DIAGS_WDIAG_MU_MSB  , 0xFFFFFFFF & (mu>>32) );
	wdiag_write( WRC_DIAGS_WDIAG_MU_LSB  , 0xFFFFFFFF &  mu );
	wdiag_write( WRC_DIAGS_WDIAG_DMS_MSB , 0xFFFFFFFF & (dms>>32) );
	wdiag_write( WRC_DIAGS_WDIAG_DMS_LSB , 0xFFFFFFFF &  dms );
	wdiag_write( WRC_DIAGS_WDIAG_ASYM    , asym );
	wdiag_write( WRC_DIAGS_WDIAG_CKO     , cko );
	wdiag_write( WRC_DIAGS_WDIAG_SETP    , setp );
	wdiag_write( WRC_DIAGS_WDIAG_UCNT    , ucnt );
}

void wdiags_write_port_state(int link, int locked)
{
	uint32_t val = 0;

	val  = link   ? WRC_DIAGS_WDIAG_PSTAT_LINK   : 0;
	val |= locked ? WRC_DIAGS_WDIAG_PSTAT_LOCKED : 0;
	wdiag_write( WRC_DIAGS_WDIAG_PSTAT    , val );

	//pp_printf("wdiags_write_port_state: %x\n", val );
}

void wdiags_write_ptp_state(uint8_t ptpstate)
{
	wdiag_write( WRC_DIAGS_WDIAG_PTPSTAT, ptpstate << WRC_DIAGS_WDIAG_PTPSTAT_PTPSTATE_SHIFT );
}

void wdiags_write_aux_state(uint32_t aux_states)
{
	wdiag_write( WRC_DIAGS_WDIAG_ASTAT, aux_states << WRC_DIAGS_WDIAG_ASTAT_AUX_SHIFT );
}

void wdiags_write_cnts(uint32_t tx, uint32_t rx, uint32_t rx_errors)
{
	wdiag_write( WRC_DIAGS_WDIAG_TXFCNT, tx);
	wdiag_write( WRC_DIAGS_WDIAG_RXFCNT, rx);
	wdiag_write( WRC_DIAGS_WDIAG_RX_ERR_CNT, rx_errors);
}

void wdiags_write_time(uint64_t sec, uint32_t nsec)
{
	wdiag_write( WRC_DIAGS_WDIAG_SEC_MSB, 0xFFFFFFFF & (sec>>32) );
	wdiag_write( WRC_DIAGS_WDIAG_SEC_LSB, 0xFFFFFFFF &  sec );
	wdiag_write( WRC_DIAGS_WDIAG_NS,       nsec );
}

void wdiags_write_temp(uint32_t temp)
{
	wdiag_write( WRC_DIAGS_WDIAG_TEMP, temp );
}

void wdiags_init()
{
	int i;

	for( i = 0; i < 64; i++ )
		wdiag_write( i * 4, 0 );

	wdiag_write( WRC_DIAGS_VER, WDIAGS_VERSION );
}

void wdiags_write_aux_clock_details( int clk_id, uint32_t mode, uint32_t phase, int enabled, int ready )
{
	uint32_t reg;
	switch(clk_id)
	{
		case 0: reg = WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT; break;
		case 1: reg = WRC_DIAGS_WDIAG_AUX1_DETAIL_STAT; break;
		case 2: reg = WRC_DIAGS_WDIAG_AUX2_DETAIL_STAT; break;
		case 3: reg = WRC_DIAGS_WDIAG_AUX3_DETAIL_STAT; break;
		default: return;
	}

	uint32_t v = 0;

	v |= mode << WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_MODE_SHIFT;
	v |= (enabled ? WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_ENABLED : 0);
	v |= (ready ? WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_LOCKED : 0);
	v |= (phase << WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_PHASE_SHIFT) & WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_PHASE_MASK;

	wdiag_write( reg, v );
}

void wdiags_write_bitslide(int bitslide)
{
	wdiag_write( WRC_DIAGS_WDIAG_BITSLIDE, bitslide );
}

void wdiags_write_ptp_deltas( int dtxm, int drxm, int dtxs, int drxs )
{
	wdiag_write( WRC_DIAGS_WDIAG_DELTA_RX_M, drxm );
	wdiag_write( WRC_DIAGS_WDIAG_DELTA_RX_S, drxs );
	wdiag_write( WRC_DIAGS_WDIAG_DELTA_TX_M, dtxm );
	wdiag_write( WRC_DIAGS_WDIAG_DELTA_TX_S, dtxs );
}