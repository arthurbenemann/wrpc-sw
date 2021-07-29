/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo <aurelio@aureliocolosimo.it>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

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
#include <dev/endpoint.h>
#include <dev/netif.h>
#include <dev/wdiags.h>
#include "sensors.h"
#include "wrc_ptp.h"
#include "hal_exports.h"
#include "lib/ipv4.h"
#include "shell.h"
#include "revision.h"
#include "hw/wrc_diags_regs.h"
#include "hw/wrc_diags_regs_v1.h"

#define WRC_MONITOR_REFRESH_PERIOD (1 * TICS_PER_SECOND)
#define WRC_DIAG_REFRESH_PERIOD (1 * TICS_PER_SECOND)

extern struct pp_servo servo;
extern struct pp_instance ppi_static;
struct pp_instance *ppi = &ppi_static;
const char *ptp_unknown_str= "unknown";

static void wrc_mon_std_servo(void);
int wrc_wr_diags(void);

#define PRINT64_FACTOR	1000000000LL
static char* print64(uint64_t x, int align)
{
	uint32_t h_half, l_half;
	static char buf[2*10+1];	//2x 32-bit value + \0

	if (x < PRINT64_FACTOR)
		if (align)
			sprintf(buf, "%20u", (uint32_t)x);
		else
			sprintf(buf, "%u", (uint32_t)x);
	else {
		l_half = __div64_32(&x, PRINT64_FACTOR);
		h_half = (uint32_t) x;
		if (align)
			sprintf(buf, "%11u%09u", h_half, l_half);
		else
			sprintf(buf, "%u%09u", h_half, l_half);
	}
	return buf;
}


static const char* wrc_ptp_state(void)
{
	struct pp_state_table_item *ip = NULL;
	for (ip = pp_state_table; ip->state != PPS_END_OF_TABLE; ip++) {
		if (ip->state == ppi->state)
			break;
	}

	if(!ip)
		return ptp_unknown_str;
	return ip->name;
}

static int wrc_mon_status(void)
{
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;

	cprintf(C_BLUE, "\n\nPTP status: ");
	cprintf(C_WHITE, "%s", wrc_ptp_state());

	if ((!s->flags & WR_FLAG_VALID) || (ppi->state != PPS_SLAVE)) {
		cprintf(C_RED,
			"\n\nSync info not valid\n");
		return 0;
	}

	/* show_servo */
	cprintf(C_BLUE, "\n\nSynchronization status:\n");

	return 1;
}


int wrc_mon_gui(void)
{
	static uint32_t last_jiffies;
	static uint32_t last_servo_count;
	struct hal_port_state state;
	int tx, rx;
	struct spll_aux_clock_status aux_stat;
	uint64_t sec;
	uint32_t nsec;
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;
	int64_t crtt;
	int64_t total_asymmetry;
	char buf[20];
	int n_out, i;

	if (!last_jiffies)
		last_jiffies = timer_get_tics() - 1 -  WRC_MONITOR_REFRESH_PERIOD;
	if (time_before(timer_get_tics(), last_jiffies + WRC_MONITOR_REFRESH_PERIOD)
		&& last_servo_count == s->update_count)
		return 0;
	last_jiffies = timer_get_tics();
	last_servo_count = s->update_count;

	term_clear();

	cprintf(C_BLUE, "WR PTP Core Sync Monitor %s", build_revision);
	cprintf(C_GREY, "\nEsc = exit");

	shw_pps_gen_get_time(&sec, &nsec);

	cprintf(C_BLUE, "\n\nTAI Time:                  ");
	cprintf(C_WHITE, "%s", format_time(sec, TIME_FORMAT_LEGACY));

	/*show_ports */
	wrpc_get_port_state(&state, NULL);
	cprintf(C_BLUE, "\n\nLink status:");

	int ndevs = netif_get_device_count();

	for( i = 0 ; i < ndevs; i++ )
	{
		struct wrc_netif_device *ndev = netif_get_device( i );
		cprintf(C_WHITE, "\n%-5s: ", ndev->name );
		if ( ndev->link_state == NETIF_LINK_UP )
			cprintf(C_GREEN, "Link up   ");
		else
			cprintf(C_RED,   "Link down ");

		if( i == 0 ) // fixme: independent rx/tx stats for each interface
		{
			int rx_er;
			minic_get_stats(&tx, &rx, &rx_er);
			cprintf(C_GREY, "(RX: %d, TX: %d, RX errors: %d)", rx, tx, rx_er);
		}
	}

	
	if (!state.state) {
		return 1;
	}

	if (HAS_IP) {
		uint8_t ip[4];

		cprintf(C_WHITE, " IPv4: ");
		getIP(ip);
		format_ip(buf, ip);
		switch (ip_status) {
		case IP_TRAINING:
			cprintf(C_RED, "BOOTP running");
			break;
		case IP_OK_BOOTP:
			cprintf(C_GREEN, "%s (from bootp)", buf);
			break;
		case IP_OK_STATIC:
			cprintf(C_GREEN, "%s (static assignment)", buf);
			break;
		}
	}

	cprintf(C_GREY, "\nMode: ");

	if (!WR_DSPOR(ppi)->wrModeOn) {
		cprintf(C_RED, "WR Off");
		wrc_mon_std_servo();
		return 1;
	}

	switch (ptp_mode) {
	case WRC_MODE_GM:
	case WRC_MODE_MASTER:
		cprintf(C_WHITE, "WR Master  ");
		break;
	case WRC_MODE_SLAVE:
		cprintf(C_WHITE, "WR Slave   ");
		break;
	default:
		cprintf(C_RED,   "WR Unknown ");
	}

	if (state.locked)
		cprintf(C_GREEN, "Locked ");
	else
		cprintf(C_RED,   "NoLock ");
	if (state.calib.rx_calibrated && state.calib.tx_calibrated)
		cprintf(C_GREEN, "Calibrated");
	else
		cprintf(C_RED, "Uncalibrated");


	if (wrc_mon_status() == 0)
		return 0;

	cprintf(C_GREY, "Servo state:               ");
	cprintf(C_WHITE, "%s\n", s->servo_state_name);
	cprintf(C_GREY, "Phase tracking:            ");
	if (s->tracking_enabled)
		cprintf(C_GREEN, "ON\n");
	else
		cprintf(C_RED, "OFF\n");
	/* sync source not implemented */
	/*cprintf(C_GREY, "Synchronization source:    ");
	cprintf(C_WHITE, "%s\n", cur_servo_state.sync_source);*/

	spll_get_num_channels(NULL, &n_out);



	for(i = 0; i < n_out - 1; i++) {
		cprintf(C_GREY, "Aux clock %d status:        ", i);

		aux_stat = spll_get_aux_status(i);

		if (aux_stat.flags & SPLL_AUX_SLAVE_ENABLED)
			cprintf(C_GREEN, "enabled");

		if (aux_stat.flags & SPLL_AUX_MONITOR_ENABLED )
			cprintf(C_GREEN, "monitor");

		if (aux_stat.flags & SPLL_AUX_SLAVE_LOCKED)
			cprintf(C_GREEN, ", locked");

		if( aux_stat.flags & SPLL_AUX_MONITOR_READY )
		{
			cprintf(C_GREEN, ", ready");
			cprintf(C_WHITE, " (AUX-to-WR offset: %d ps)", aux_stat.phase );
		}
		
		pp_printf("\n");

	}

	cprintf(C_BLUE, "\nTiming parameters:\n");

	cprintf(C_GREY, "Round-trip time (mu): ");
	cprintf(C_WHITE, "%s ps\n", print64(s->picos_mu, 1));
	cprintf(C_GREY, "Master-slave delay:   ");
	cprintf(C_WHITE, "%s ps\n", print64(s->delta_ms, 1));

	cprintf(C_GREY, "Master PHY delays:           ");
	cprintf(C_WHITE, "TX: %9d ps, RX: %9d ps\n",
		(int32_t) s->delta_tx_m,
		(int32_t) s->delta_rx_m);

	cprintf(C_GREY, "Slave PHY delays:            ");
	cprintf(C_WHITE, "TX: %9d ps, RX: %9d ps\n",
		(int32_t) s->delta_tx_s,
		(int32_t) s->delta_rx_s);
	total_asymmetry = s->picos_mu - 2LL * s->delta_ms;
	cprintf(C_GREY, "Total link asymmetry:");
	cprintf(C_WHITE, "%21d ps\n", (int32_t) (total_asymmetry));

	crtt = s->picos_mu - s->delta_tx_m - s->delta_rx_m
		- s->delta_tx_s - s->delta_rx_s;
	cprintf(C_GREY, "Cable rtt delay:      ");
	cprintf(C_WHITE, "%s ps\n", print64(crtt, 1));

	cprintf(C_GREY, "Clock offset:");
	cprintf(C_WHITE, "%29d ps\n", (int32_t) (s->offset));

	cprintf(C_GREY, "Phase setpoint:");
	cprintf(C_WHITE, "%27d ps\n", (s->cur_setpoint));

	cprintf(C_GREY, "Skew:     ");
	/* precision is limited to 32 */
	cprintf(C_WHITE, "%32d ps\n", (int32_t) (s->skew));

	cprintf(C_GREY, "Update counter:");
	cprintf(C_WHITE, "%27d\n", (int32_t) (s->update_count));

	cprintf(C_GREY, "Extra stats: ");
	cprintf(C_WHITE, " Sync packet errors: %d followup errors: %d servo restarts: %d\n", ppi->stats.sync_errors, ppi->stats.followup_errors, ppi->stats.servo_restarts);

	return 0;
}

static inline void cprintf_time(int color, struct pp_time *time)
{
	int s, ns;

	s = (int)time->secs;
	ns = (int)(time->scaled_nsecs >> 16);
	if (s > 0 || (s == 0 && ns >= 0)) {
		cprintf(color, "%2i.%09i s", s, ns);
	} else { /* negative */
		if (time->secs == 0)
			cprintf(color, "-%i.%09i s", s, -ns);
		else
			cprintf(color, "%i.%09i s", s, -ns);
	}
}

static void wrc_mon_std_servo(void)
{
	if (wrc_mon_status() == 0)
		return;

	cprintf(C_GREY, "\nClock offset:                 ");

	if (DSCUR(ppi)->offsetFromMaster.secs)
		cprintf_time(C_WHITE, &DSCUR(ppi)->offsetFromMaster);
	else {
		cprintf(C_WHITE, "%9i ns",
			(int)(DSCUR(ppi)->offsetFromMaster.scaled_nsecs >> 16));

		cprintf(C_GREY, "\nOne-way delay averaged:       ");
		cprintf(C_WHITE, "%9i ns",
			(int)(DSCUR(ppi)->meanPathDelay.scaled_nsecs >> 16));

		cprintf(C_GREY, "\nObserved drift:               ");
		cprintf(C_WHITE, "%9i ns",SRV(ppi)->obs_drift);
	}
}


/* internal "last", exported to shell command */
uint32_t wrc_stats_last;

int wrc_log_stats(void)
{
	struct hal_port_state state;
	int tx, rx, rx_errors;
	struct spll_aux_clock_status aux_stat;
	uint64_t sec;
	uint32_t nsec;
	struct wr_servo_state *s =
			&((struct wr_data *)ppi->ext_data)->servo_state;
	static uint32_t last_jiffies;
	int n_out;
	int i;

	if (!wrc_stat_running)
		return 0;

	if (!last_jiffies)
		last_jiffies = timer_get_tics() - 1 -  WRC_MONITOR_REFRESH_PERIOD;
	/* stats update condition for Slave mode */
	if (wrc_stats_last == s->update_count && ptp_mode==WRC_MODE_SLAVE)
		return 0;
	/* stats update condition for Master mode */
	if (time_before(timer_get_tics(), last_jiffies + WRC_MONITOR_REFRESH_PERIOD) &&
			ptp_mode != WRC_MODE_SLAVE)
		return 0;
	last_jiffies = timer_get_tics();

	/* Print only one time */
	if(wrc_stat_running == -1)
		wrc_stat_running = 0;

	wrc_stats_last = s->update_count;

	shw_pps_gen_get_time(&sec, &nsec);
	wrpc_get_port_state(&state, NULL);
	minic_get_stats(&tx, &rx, &rx_errors);
	pp_printf("lnk:%d rx:%d tx:%d ", state.state, rx, tx);
	pp_printf("lock:%d ", state.locked ? 1 : 0);
	pp_printf("ptp:%s ", wrc_ptp_state());
	if(ptp_mode == WRC_MODE_SLAVE) {
		pp_printf("sv:%d ", (s->flags & WR_FLAG_VALID) ? 1 : 0);
		pp_printf("ss:'%s' ", s->servo_state_name);
	}

	spll_get_num_channels(NULL, &n_out);

	for(i = 0; i < n_out; i++) {
		aux_stat = spll_get_aux_status(i);
		pp_printf("aux%d:%08x%08x ", i, aux_stat.flags, aux_stat.phase);
	}
	
	/* fixme: clock is not always 125 MHz */
	pp_printf("sec:%d nsec:%d ", (uint32_t) sec, nsec);
	if(ptp_mode == WRC_MODE_SLAVE) {
		pp_printf("mu:%s ", print64(s->picos_mu, 0));
		pp_printf("dms:%s ", print64(s->delta_ms, 0));
		pp_printf("dtxm:%d drxm:%d ", (int32_t) s->delta_tx_m,
			(int32_t) s->delta_rx_m);
		pp_printf("dtxs:%d drxs:%d ", (int32_t) s->delta_tx_s,
			(int32_t) s->delta_rx_s);
		int64_t total_asymmetry = s->picos_mu -
				  2LL * s->delta_ms;
		pp_printf("asym:%d ", (int32_t) (total_asymmetry));
		pp_printf("crtt:%s ", print64(s->picos_mu -
					s->delta_tx_m -
					s->delta_rx_m -
					s->delta_tx_s -
					s->delta_rx_s, 0));
		pp_printf("cko:%d ", (int32_t) (s->offset));
		pp_printf("setp:%d ", (int32_t) (s->cur_setpoint));
		pp_printf("ucnt:%d ", (int32_t) s->update_count);
		pp_printf("bslide:%d ", ep_get_bitslide(&wrc_endpoint_dev));
	}
	
	pp_printf("hd:%d md:%d ad:%d ", spll_get_dac(-1), spll_get_dac(0),
		spll_get_dac(1));

	if (1) {
		
		struct wrc_sensor *s = wrc_sensor_find_by_type( WRC_SENSOR_TEMP_CELSIUS );

		if( s )
		{
			pp_printf("temp: %d degC", s->value );
		}
	}

	pp_printf("\n");

	return 1;
}

/* this is to avoid breaking wrc_wr_diags and its clone */
uint32_t wrc_temp_get(char *name)
{
	struct wrc_sensor *s = wrc_sensor_find_by_type( WRC_SENSOR_TEMP_CELSIUS );
	return s->value;
}

int wrc_ptp_get_servo_state( void )
{
	struct wr_servo_state *ss =
		&((struct wr_data *)ppi->ext_data)->servo_state;
		int32_t asym   = (int32_t)(ss->picos_mu-2LL * ss->delta_ms);
		int wr_mode    = (ss->flags & WR_FLAG_VALID) ? 1 : 0;
	return  ss->state;
}

int wrc_ptp_get_state( void )
{
	return ppi->state;
}

int wrc_wr_diags(void)
{
	struct hal_port_state ps;
	static uint32_t last_jiffies;
	int tx, rx, rx_errors;
	uint64_t sec;
	uint32_t nsec;
	int n_out;
	uint32_t aux_stat=0;
	int temp=0, valid=0, snapshot=0,i;

	valid    = wdiag_get_valid();
	snapshot = wdiag_get_snapshot();

	/* if the data is snapshot and there is already valid data, do not
	 * refresh */
	if(valid & snapshot)
	      return 0;

	/* ***************** lock data from reading by user **************** */
	if (!last_jiffies)
		last_jiffies = timer_get_tics() - 1 -  WRC_DIAG_REFRESH_PERIOD;
	/* stats update condition */
	if (time_before(timer_get_tics(), last_jiffies + WRC_DIAG_REFRESH_PERIOD))
		return 0;
	last_jiffies = timer_get_tics();

	/* ***************** lock data from reading by user **************** */
	wdiag_set_valid(0);
	
	/* frame statistics */
	minic_get_stats(&tx, &rx, &rx_errors);
	wdiags_write_cnts(tx,rx,rx_errors);

	/* local time */
	shw_pps_gen_get_time(&sec, &nsec);
	wdiags_write_time(sec, nsec);
	
	/* port state (from hal) */
	wrpc_get_port_state(&ps, NULL);
	wdiags_write_port_state((ps.state  ? 1 : 0), (ps.locked ? 1 : 0));

	/* port PTP State (from ppsi)
	* see:
		ppsi/proto-ext-whiterabbit/wr-constants.h
		ppsi/include/ppsi/ieee1588_types.h
	0  : none
	1  : PPS_INITIALIZING
	2  : PPS_FAULTY
	3  : PPS_DISABLED
	4  : PPS_LISTENING
	5  : PPS_PRE_MASTER
	6  : PPS_MASTER
	7  : PPS_PASSIVE
	8  : PPS_UNCALIBRATED
	9  : PPS_SLAVE
	100: WRS_PRESENT
	101: WRS_S_LOCK
	102: WRS_M_LOCK
	103: WRS_LOCKED
	104, 108-116:WRS_CALIBRATION
	105: WRS_CALIBRATED
	106: WRS_RESP_CALIB_REQ
	107: WRS_WR_LINK_ON
	*/
	wdiags_write_ptp_state((uint8_t )ppi->state);

	/* servo state (if slave)s */
	if(ptp_mode == WRC_MODE_SLAVE){
		struct wr_servo_state *ss =
			&((struct wr_data *)ppi->ext_data)->servo_state;
		int32_t asym   = (int32_t)(ss->picos_mu-2LL * ss->delta_ms);
		int wr_mode    = (ss->flags & WR_FLAG_VALID) ? 1 : 0;
		int servostate =  ss->state;
		/* see ppsi/proto-ext-whiterabbit/wr-constants.c:
		0: WR_UNINITIALIZED = 0,
		1: WR_SYNC_NSEC,
		2: WR_SYNC_TAI,
		3: WR_SYNC_PHASE,
		4: WR_TRACK_PHASE,
		5: WR_WAIT_OFFSET_STABLE */

		wdiags_write_servo_state(wr_mode, servostate, ss->picos_mu,
					 ss->delta_ms, asym, ss->offset,
					 ss->cur_setpoint,ss->update_count, 0, 0); // fixme: add wdiags v2

		wdiags_write_ptp_deltas( ss->delta_tx_m, ss->delta_rx_m, ss->delta_tx_s, ss->delta_rx_s );
	}

	/* auxiliar channels (if any) */
	spll_get_num_channels(NULL, &n_out);
	if (n_out > 8) n_out = 8; /* hardware limit. */
	for(i = 0; i < n_out; i++) {
		aux_stat |= (( SPLL_AUX_SLAVE_LOCKED | SPLL_AUX_MONITOR_READY ) & spll_get_aux_status(i).flags) << i;
	}

	wdiags_write_aux_state(aux_stat);


	
	for(i = 0; i < n_out - 1; i++)
	{
		int mode;
		int enabled;
		int ready;
		struct spll_aux_clock_status st = spll_get_aux_status( i );

		if( st.mode == SPLL_AUX_MODE_SLAVE )
		{
			mode = 0;
			enabled = st.flags & SPLL_AUX_SLAVE_ENABLED ? 1 : 0;
			ready = st.flags & SPLL_AUX_SLAVE_LOCKED ? 1 : 0 ;
		}
		else
		{
			mode = 1;
			enabled = st.flags & SPLL_AUX_MONITOR_ENABLED ? 1 : 0;
			ready = st.flags & SPLL_AUX_MONITOR_READY ? 1 : 0 ;
		}

		wdiags_write_aux_clock_details( i, mode, st.phase, enabled, ready );
	}


	wdiags_write_bitslide( ep_get_bitslide(&wrc_endpoint_dev) );

	/* temperature */
	temp = wrc_temp_get("pcb");
	wdiags_write_temp(temp);

	/* **************** unlock data from reading by user  ************** */
	wdiag_set_valid(1);
	
	return 1;
}

/*
 * this function can be used to factor out most of the stuff
 * in wrc_wr_diags
 * either this, or make syscon.c wdiags_* functions not depend on a
 * global...
 *
 * FIXME: add wrc_diags_dump to a header!!!
 * FIXME: refactor wdiags_* function in dev/syscon.c so that they
 *  do not write directly to syscon, but to an arbitrary address.
 *  That will clean the code here *enormously*
 */
int wrc_diags_dump(struct wrc_diags_regs_v1 *buf)
{
	struct hal_port_state ps;
	int tx, rx;
	uint64_t sec;
	uint32_t nsec;
	uint32_t aux_stat;
	int i, temp, n_out;

	buf->VER = 0x12345678;
	buf->CTRL = 0xcafebabe;
	/* frame statistics */
	minic_get_stats(&tx, &rx, NULL);
	buf->WDIAG_TXFCNT = tx;
	buf->WDIAG_RXFCNT = rx;

	/* local time */
	shw_pps_gen_get_time(&sec, &nsec);
	buf->WDIAG_SEC_MSB = 0xFFFFFFFF & (sec>>32);
	buf->WDIAG_SEC_LSB = 0xFFFFFFFF &  sec;
	buf->WDIAG_NS      = nsec;

	/* port state (from hal) */
	wrpc_get_port_state(&ps, NULL);
	buf->WDIAG_PSTAT  = (ps.state ? WRC_DIAGS_WDIAG_PSTAT_LINK : 0);
	buf->WDIAG_PSTAT |= (ps.locked ? WRC_DIAGS_WDIAG_PSTAT_LOCKED : 0);

	/* port PTP State (from ppsi) */
	buf->WDIAG_PTPSTAT = SYSC_WDIAG_PTPSTAT_PTPSTATE_W((uint8_t)ppi->state);

	/* servo state (if slave)s */
	if(ptp_mode == WRC_MODE_SLAVE) {
		struct wr_servo_state *ss =
			&((struct wr_data *)ppi->ext_data)->servo_state;
		int32_t asym   = (int32_t)(ss->picos_mu-2LL * ss->delta_ms);
		int wr_mode    = (ss->flags & WR_FLAG_VALID) ? 1 : 0;
		int servostate =  ss->state;
		uint64_t mu = ss->picos_mu;
		uint64_t dms = ss->delta_ms;

		buf->WDIAG_SSTAT   = wr_mode ? WRC_DIAGS_WDIAG_SSTAT_WR_MODE : 0;
		buf->WDIAG_SSTAT  |= SYSC_WDIAG_SSTAT_SERVOSTATE_W(servostate);
		buf->WDIAG_MU_MSB  = 0xFFFFFFFF & (mu>>32);
		buf->WDIAG_MU_LSB  = 0xFFFFFFFF &  mu;
		buf->WDIAG_DMS_MSB = 0xFFFFFFFF & (dms>>32);
		buf->WDIAG_DMS_LSB = 0xFFFFFFFF &  dms;
		buf->WDIAG_ASYM    = asym;
		buf->WDIAG_CKO     = ss->offset;
		buf->WDIAG_SETP    = ss->cur_setpoint;
		buf->WDIAG_UCNT    = ss->update_count;
	}
	/* auxiliar channels (if any) */
	spll_get_num_channels(NULL, &n_out);
	if (n_out > 8) n_out = 8; /* hardware limit. */
	aux_stat = 0;
	for(i = 0; i < n_out; i++) {
		aux_stat |= (( SPLL_AUX_SLAVE_LOCKED | SPLL_AUX_MONITOR_READY ) & spll_get_aux_status(i).flags) << i;
	}
	buf->WDIAG_ASTAT = WRC_DIAGS_WDIAG_ASTAT_AUX_W(aux_stat);

	/* temperature */
	temp = wrc_temp_get("pcb");
	buf->WDIAG_TEMP = temp;

	return 1;
}
