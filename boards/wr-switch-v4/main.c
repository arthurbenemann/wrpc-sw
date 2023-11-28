/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012,2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include "dev/console.h"
#include "softpll_ng.h"
#include "minipc.h"
#include "revision.h"
#include "system_checks.h"
#include "gpio-wrs.h"

#include "dev/si57x.h"

#define HW_NAME_LENGTH 5

#include <wrc_global.h>


struct wrc_global_link wrc_global_link = {
	.version = WRC_G_LINK_VERSION,
	.vlan = 0,
	.ip_state = IP_TRAINING,
};

const struct wrc_global wrc_global = {
	.magic = WRC_G_MAGIC,
	.version = WRC_G_VERSION,
	.global_link = &wrc_global_link,
	.task_list_max = WRC_MAX_TASKS,
	.task_list = tasks,
	.temp_group_list_max = 0,
	.temp_group_list = NULL,
	.softpll =NULL,
	.pll_fifo = NULL,
	.config = NULL,
	.sfp_info = NULL
};

//extern struct spll_stats stats;

int scb_ljd_present = 0;

struct rts_10g_board {
	struct wr_si57x_interface_device si57x;
} board;

#define RTS_MBOX_SIZE 0x1000
#define RTS_MBOX_ADDR (DEV_BASE + 0)

volatile uint8_t *mbox_mem = (volatile uint8_t*)( RTS_MBOX_ADDR );

int rts_debug_command(int command, int value)
{
	return 0;
}

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	console_init();
}

int board_init()
{
	uint32_t f_xtal;

	board_dbg("board_init()\n");
	wr_si57x_interface_init( &board.si57x, BASE_SI57X_INTERFACE, SI57X_I2C_ADDR );
	
	#if 0
	if( !wr_si57x_probe_chip( &board.si57x ) )
	{
		pp_printf("Si57x NOT FOUND. Can't continue...\n");
		return -1;
	}
	#endif

	si57x_reset( &board.si57x );
	si57x_get_xtal_frequency( &board.si57x, &f_xtal );

	board_dbg("Si57x xtal freq: %d Hz\n", f_xtal );
	si57x_set_frequency( &board.si57x, f_xtal, 125000000, 10 );
}

int main(void)
{
	uint32_t start_tics = timer_get_tics();

	check_reset();

	mbox_mem[0] = 0xca;
	mbox_mem[1] = 0xfe;
	mbox_mem[2] = 0xba;
	mbox_mem[3] = 0xbe;

	//stats.start_cnt++;

	_endram = ENDRAM_MAGIC;
	wrs_gpio_init();
	console_init();
	pp_printf("\n");
	pp_printf("WR Switch 10G Proto Real Time Subsystem (c) CERN 2011 - 2020\n");
	//pp_printf("Revision: %s, built: %s %s.\n",
	  //    build_revision, build_date, build_time);

	board_init();
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
