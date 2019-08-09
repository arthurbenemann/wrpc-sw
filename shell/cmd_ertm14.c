/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "dev/clock_monitor.h"
#include "shell.h"

extern struct wb_clock_monitor_device ertm14_cmon;

const char* clock_names[] = { "clk_dmtd", "clk_sys", "clk_tx1", "clk_tx2", "clk_rx" };

static int cmd_ertm(const char *args[])
{
	int i;

	if (!strcasecmp(args[0], "ct")) {
        struct wb_clock_monitor_device* cm = &board.ertm14_cmon;
		pp_printf("eRTM clock test:\n");
        wb_cm_restart( cm );
        usleep(3000000);
        wb_cm_read( cm );

        for( i = 0; i < cm->n_channels; i++ )
        {
            if( cm->freq_valid_mask & (1<<i)) 
            {
                pp_printf("%d [%s] : %d Hz\n", i, clock_names[i], cm->freqs[i]);
            }
         }
	} else if (!strcasecmp(args[0], "dds")) {
        
    } else if (!strcasecmp(args[0], "rf-distr")) {
        
    } else if (!strcasecmp(args[0], "clk-distr")) {
        
    }
}

DEFINE_WRC_COMMAND(ertm) = {
	.name = "ertm",
	.exec = cmd_ertm,
};
