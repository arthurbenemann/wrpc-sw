/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>

#include <stdarg.h>

#include <wrc.h>
#include <temperature.h>
#include <dev/w1.h>
#include <dev/syscon.h>
#include <dev/console.h>
#include <dev/endpoint.h>
#include <dev/minic.h>
#include <dev/pps_gen.h>
#include <ptpd_netif.h>
#include <dev/i2c.h>
#include <storage.h>
#include <softpll_ng.h>
#include <dev/onewire.h>
#include <dev/pps_gen.h>
#include <shell.h>
#include <lib/ipv4.h>
#include <dev/rxts_calibrator.h>
#include <dev/flash.h>
#include <dev/gpio.h>

#include <wrc_ptp.h>
#include <system_checks.h>
#include <ppsi/ppsi.h>

#include <TuneGuido.h>

#ifdef CONFIG_DAC_LOG
#include "dev/dac_log.h"
#endif

#ifdef CONFIG_LATENCY_PROBE
#include "lib/latency.h"
#endif

#ifdef CONFIG_LLDP
#include "lib/lldp.h"
#endif

char wrc_hw_name[HW_NAME_LENGTH];

uint32_t cal_phase_transition = 2389;

// dirty Hack




int wrc_vlan_number = CONFIG_VLAN_NR;

// fixme: this probably deserves to be moved to another file...
static int prev_ptp_mode;
static int prev_ptp_state;
static int prev_servo_state;
static int prev_timing_ok;

int wrc_wr_diags(void); // fixme: move the header

static void wrc_initialize(void)
{
#ifdef CONFIG_USE_SDB
	sdb_find_devices();
#endif

	console_init();
	timer_init(1);
	spll_very_init();
	usleep_init();

	wrc_board_early_init();

	pp_printf("WR Core: starting up...\n");
	Kphpsec=&pguido;
	pp_printf("Val of %d\n",*Kphpsec);
	get_hw_name(wrc_hw_name);

	net_rst();
	ep_init();
	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();

	storage_load_calibration();

	wrc_ptp_init();
	/* try reading t24 phase transition from EEPROM */
	calib_t24p(WRC_MODE_MASTER, &cal_phase_transition);
	shell_init();
	shell_register_commands();

	wrc_board_init();

	_endram = ENDRAM_MAGIC;

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();

	wrc_tasks_accounting_init();
	wrc_board_create_tasks();
}

int link_status;

static int is_link_up(void)
{
	return link_status == LINK_UP;
}

static int wrc_check_link(void)
{
	static int prev_state = 0;
	int state = ep_link_up(NULL);
	int rv = 0;

	if (!prev_state && state) {
		wrc_verbose("Link up.\n");
		event_post( WRC_EVENT_LINK_UP );
		gen_gpio_out(&pin_sysc_led_link, 1);
		sfp_match();
		wrc_ptp_start();
		link_status = LINK_WENT_UP;
		rv = 1;
	} else if (prev_state && !state) {
		wrc_verbose("Link down.\n");
		prev_timing_ok = 0;
		event_post( WRC_EVENT_LINK_DOWN );
		gen_gpio_out(&pin_sysc_led_link, 0);
		link_status = LINK_WENT_DOWN;
		wrc_ptp_stop();
		rv = 1;
		/* special case */
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, SPLL_FLAG_ALIGN_PPS);
		shw_pps_gen_enable_output(0);

	} else
		link_status = (state ? LINK_UP : LINK_DOWN);
	prev_state = state;

	return rv;
}

static int ui_update(void)
{
	return shell_interactive();
}

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
#ifdef CONFIG_USE_SDB
	sdb_find_devices();
#endif
	uart_init_hw();
	timer_init(1);
}


/* count uptime, in seconds, for remote polling */
static uint32_t uptime_lastj;
static void init_uptime(void)
{
	uptime_lastj = timer_get_tics();
}

static int update_uptime(void)
{
	extern uint32_t uptime_sec;
	uint32_t j;
	static uint32_t fraction = 0;

	j = timer_get_tics();
	fraction += j - uptime_lastj;
	uptime_lastj = j;
	if (fraction > TICS_PER_SECOND) {
		fraction -= TICS_PER_SECOND;
		uptime_sec++;
		return 1;
	}
	return 0;
}

int wrc_is_timing_up()
{
	return prev_timing_ok;
}

static void wrc_dispatch_ptp_events_init(void)
{
	prev_ptp_mode = -1;
	prev_ptp_state = -1;
	prev_servo_state = -1;
	prev_timing_ok = 0;
}

static int wrc_dispatch_ptp_events_poll(void)
{
	extern struct pp_instance ppi_static;
	struct pp_instance *ppi = &ppi_static;
	struct wr_servo_state *ss = &((struct wr_data *)ppi->ext_data)->servo_state;

	int mode = wrc_ptp_get_mode();

	if( mode != prev_ptp_mode )
	{
		main_dbg("PTP mode changed.\n");
		prev_timing_ok = 0;
		event_post( WRC_EVENT_PTP_MODE_CHANGED );
	}

	prev_ptp_mode = mode;

	// observe the PTP state machine transitions and the servo state - and depending on the mode of
	// operation (master/slave), send the 'Timing up'/'Timing down' events.
	if( mode == WRC_MODE_MASTER )
	{
		if( ppi->state == PPS_MASTER && prev_ptp_state != PPS_MASTER )
		{
			prev_timing_ok = 1;
			event_post( WRC_EVENT_TIMING_UP );
		}
		else if ( ppi->state != PPS_MASTER && prev_ptp_state == PPS_MASTER )
		{
			prev_timing_ok = 0;
			event_post( WRC_EVENT_TIMING_DOWN );
		}
	}
	else if ( mode == WRC_MODE_SLAVE )
	{
		if( ppi->state == PPS_SLAVE )
		{
			if( ss->state == WR_TRACK_PHASE && prev_servo_state != WR_TRACK_PHASE )
			{
				prev_timing_ok = 1;
				event_post( WRC_EVENT_TIMING_UP );
			}
			else if( ss->state != WR_TRACK_PHASE && prev_servo_state == WR_TRACK_PHASE )
			{
				prev_timing_ok = 0;
				event_post( WRC_EVENT_TIMING_DOWN );
			}
		}
		else if( ppi->state != PPS_SLAVE && prev_ptp_state == PPS_SLAVE )
		{
			prev_timing_ok = 0;
			event_post( WRC_EVENT_TIMING_DOWN );
		}
	}

	prev_ptp_state = ppi->state;
	prev_servo_state = ss->state;

	return 1;
}

static void create_tasks(void)
{
	struct wrc_task *t;

	wrc_tasks_init();
	wrc_task_create( "idle", wrc_initialize, NULL );
	wrc_task_create( "check-link", NULL, wrc_check_link );
	wrc_task_create( "uptime", init_uptime, update_uptime );
	wrc_task_create( "ptp", NULL, wrc_ptp_update);
	wrc_task_create( "shell+gui", shell_boot_script, ui_update );
	wrc_task_create( "spll-bh", NULL, spll_update );
	wrc_task_create( "ptp-events", wrc_dispatch_ptp_events_init, wrc_dispatch_ptp_events_poll );
	//wrc_task_create( "temperature", wrc_temp_init, wrc_temp_refresh );

	t = wrc_task_create( "net-bh", NULL, net_bh_poll );
	wrc_task_set_enable( t, is_link_up );

#ifdef CONFIG_DAC_LOG
	wrc_task_create( "dac-logger", daclog_init, daclog_poll );
#endif

#ifdef CONFIG_IP
	t = wrc_task_create( "arp", arp_init, arp_poll );
	wrc_task_set_enable( t, is_link_up );
	t = wrc_task_create( "ipv4", ipv4_init, ipv4_poll );
	wrc_task_set_enable( t, is_link_up );
#endif

#ifdef CONFIG_LATENCY_PROBE
	extern void latency_init(void);
	extern void latency_poll(void);
	wrc_task_create( "latency-probe", latency_init, latency_poll );
#endif

#ifdef CONFIG_LLDP
	wrc_task_create( "lldp", lldp_init, lldp_poll );
#endif

#ifdef CONFIG_SNMP
	t = wrc_task_create( "snmp", snmp_init, snmp_poll );
	wrc_task_set_enable( t, is_link_up );
#endif

	wrc_task_create( "stats", NULL, wrc_log_stats );

#ifdef CONFIG_WR_DIAG
	wrc_task_create( "diags", NULL, wrc_wr_diags );
#endif
}

int main(void) __attribute__ ((weak));
int main(void)
{
	check_reset();
	create_tasks();

	/* initialization of individual tasks */
	wrc_start_all_tasks();

	for (;;) {
		// run all pending tasks
		wrc_poll_all_tasks();
		// call all event handlers
		events_dispatch();
		/* better safe than sorry */
		check_stack();
		// dirty Hack

	}
}
