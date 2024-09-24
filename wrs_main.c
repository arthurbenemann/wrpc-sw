/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012, 2015, 2023 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include "uart.h"
#include "softpll_ng.h"
#include "minipc.h"
#include "revision.h"
#include "system_checks.h"
#include "gpio-wrs.h"


int scb_ver = 33;		/* SCB version */

extern struct spll_stats stats;

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	uart_init_hw();
}

int lj_periph_id_read(void){
	int periph_id=0;

	periph_id  =  gpio_in(GPIO_LJD_PERIPH_ID_0);
	periph_id += (gpio_in(GPIO_LJD_PERIPH_ID_1)<<1);
	periph_id += (gpio_in(GPIO_LJD_PERIPH_ID_2)<<2);

	return periph_id;
}

static int lj_periph_type_read(int ljd_present,int periph_id) {
	int osc_freq  = 0;

	if (ljd_present == 0) {
		pp_printf("\n--- WRS without Low jitter peripherial detected.\n");
		return PERIPH_WRS_STD_NO_LJ;
    }

	osc_freq   =  gpio_in(GPIO_LJD_OSC_FREQ_0);
	osc_freq  += (gpio_in(GPIO_LJD_OSC_FREQ_1)<<1);
	osc_freq  += (gpio_in(GPIO_LJD_OSC_FREQ_2)<<2);

	pp_printf("\n--- WRS Low jitter peripherial detected. "
		  "OSC FREQ is %d LJ_PERIPH_ID is %d ---\n",
		  osc_freq, periph_id);
	pp_printf("Allow 1 hour of warming up before starting measurements\n");
	pp_printf("Derived LJ Peripherial type: ");
	if (osc_freq == OSC_FREQ_WRS_LJ_INT && periph_id == PERIPH_ID_WRS_FL_SYNCTECHv1_0) {
		pp_printf("WRS-FL from SyncTech 1.0\n");
		return PERIPH_WRS_FL_SYNCTECH;
	}
        if (osc_freq == OSC_FREQ_WRS_LJ_INT && periph_id == PERIPH_ID_WRS_FL_SYNCTECHv1_5) {
                pp_printf("WRS-FL from SyncTech 1.5\n");
                return PERIPH_WRS_FL_SYNCTECH;
        }
	if (osc_freq == OSC_FREQ_WRS_LJ_INT && periph_id == PERIPH_ID_WRS_LJ_SAFRAN) {
		pp_printf("WRS-LJ from Safran\n");
		return PERIPH_WRS_LJ_SAFRAN;
	}

	pp_printf("WRS with plugged Low Jitter Daughterboard\n");
	return PERIPH_WRS_STD_WITH_LJD;
}


int main(void)
{
	uint32_t start_tics = timer_get_tics();

	check_reset();
	stats.start_cnt++;
	_endram = ENDRAM_MAGIC;
	uart_init_hw();
	pp_printf("\n");
	pp_printf("WR Switch Real Time Subsystem (c) CERN 2011 - 2024\n");
	pp_printf("Revision: %s, built: %s %s by %s.\n",
		  build_revision, build_date, build_time, build_by);
	pp_printf("SCB version: %d. %s\n", scb_ver,(scb_ver>=34)?"10 MHz SMC Output.":"" );
	pp_printf("Start counter %d\n", stats.start_cnt);
	/* Low-jitter Daughterboard detection */
	ljd_present = gpio_in(GPIO_LJD_BOARD_DETECT);
	periph_id  =  lj_periph_id_read();
	lj_periph_type = lj_periph_type_read(ljd_present,periph_id);
	
	if (stats.start_cnt > 1) {
		pp_printf("!!spll does not work after restart!!\n");
		/* for sure problem is in calling second time ad9516_init,
		 * but not only */
	}

	if ((ljd_present == 1) && (lj_periph_type != PERIPH_WRS_FL_SYNCTECH))
		ad9516_init(lj_periph_type, 1);
	else
		ad9516_init(lj_periph_type, 0);
	rts_init();
	rtipc_init();
	spll_very_init();

	for(;;)
	{
		uint32_t tics = timer_get_tics();

		if (time_after(tics, start_tics + TICS_PER_SECOND/5)) {
			spll_show_stats();
			start_tics = tics;
		}

		rts_update();
		rtipc_action();
		spll_update();
		check_stack();
	}

	return 0;
}
