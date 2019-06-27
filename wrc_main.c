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
#include <w1.h>
#include <temperature.h>
#include "syscon.h"
#include "uart.h"
#include "endpoint.h"
#include "minic.h"
#include "pps_gen.h"
#include "ptpd_netif.h"
#include "i2c.h"
#include "storage.h"
#include "softpll_ng.h"
#include "onewire.h"
#include "pps_gen.h"
#include "shell.h"
#include "lib/ipv4.h"
#include "rxts_calibrator.h"
#include "flash.h"

#include "wrc_ptp.h"
#include "system_checks.h"

#ifdef CONFIG_DAC_LOG
#include "dev/dac_log.h"
#endif

#ifdef CONFIG_IP
#include "lib/arp.h"
#endif

#ifdef CONFIG_LATENCY_PROBE
#include "lib/latency.h"
#endif

#ifdef CONFIG_LLDP
#include "lib/lldp.h"
#endif

#ifdef CONFIG_SNMP
#include "lib/snmp.h"
#endif

int wrc_ui_mode = UI_SHELL_MODE;
int wrc_ui_refperiod = TICS_PER_SECOND; /* 1 sec */
int wrc_phase_tracking = 1;
char wrc_hw_name[HW_NAME_LENGTH];

uint32_t cal_phase_transition = 2389;

int wrc_vlan_number = CONFIG_VLAN_NR;

static void wrc_initialize(void)
{
	uint8_t mac_addr[6];

	sdb_find_devices();
	wrc_board_early_init();

	pp_printf("WR Core: starting up...\n");

	timer_init(1);
	get_hw_name(wrc_hw_name);

	wrc_board_init();
	
	if (HAS_GENSDBFS)
	storage_read_hdl_cfg();

	net_rst();
	ep_init();
	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable(1, 1);

	minic_init();
	shw_pps_gen_init();
	wrc_ptp_init();
	/* try reading t24 phase transition from EEPROM */
	calib_t24p(WRC_MODE_MASTER, &cal_phase_transition);
	spll_very_init();
	usleep_init();
	shell_init();

	wrc_ui_mode = UI_SHELL_MODE;
	_endram = ENDRAM_MAGIC;

	wrc_ptp_set_mode(WRC_MODE_SLAVE);
	wrc_ptp_start();

	wrc_tasks_accounting_init();
	wrc_board_create_tasks();
}

int link_status;

static int is_link_up()
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
		gpio_out(GPIO_LED_LINK, 1);
		sfp_match();
		wrc_ptp_start();
		link_status = LINK_WENT_UP;
		rv = 1;
	} else if (prev_state && !state) {
		wrc_verbose("Link down.\n");
		gpio_out(GPIO_LED_LINK, 0);
		link_status = LINK_WENT_DOWN;
		wrc_ptp_stop();
		rv = 1;
		/* special case */
		spll_init(SPLL_MODE_FREE_RUNNING_MASTER, 0, 1);
		shw_pps_gen_enable_output(0);

	} else
		link_status = (state ? LINK_UP : LINK_DOWN);
	prev_state = state;

	return rv;
}

static int ui_update(void)
{
	int ret;

	if (wrc_ui_mode == UI_GUI_MODE) {
		ret = wrc_mon_gui();
		if (uart_read_byte() == 27 || wrc_ui_refperiod == 0) {
			shell_init();
			wrc_ui_mode = UI_SHELL_MODE;
		}
	} else {
		ret = shell_interactive();
	}
	return ret;
}

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	sdb_find_devices();
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

extern void wrc_log_stats(void);

static void create_tasks()
{
	struct wrc_task *t;

	wrc_tasks_init();
	wrc_task_create( "idle", wrc_initialize, NULL );
	wrc_task_create( "check-link", NULL, wrc_check_link );
	wrc_task_create( "uptime", init_uptime, update_uptime );
	wrc_task_create( "ptp", NULL, wrc_ptp_update);
	wrc_task_create( "shell+gui", shell_boot_script, ui_update );
	wrc_task_create( "spll-bh", NULL, spll_update );
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
		wrc_poll_all_tasks();

		/* better safe than sorry */
		check_stack();
	}
}
