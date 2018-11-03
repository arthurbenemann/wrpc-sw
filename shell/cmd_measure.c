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

#include "softpll_ng.h"
#include "shell.h"

static int cmd_measure(const char *args[])
{
    const int timeout = 10000/200;
	int n = 0, i;
    int ref = atoi(args[0]);
    int chan = atoi(args[1]);
    int n_samples = atoi(args[2]);

    if( ref < 1 || ref > 5 )
    {
        pp_printf("ERROR: reference channel must be [1..5]\n");
    }

    if( chan < 1 || chan > 5 )
    {
        pp_printf("ERROR: measured channel must be [1..5]\n");
    }


	spll_init(SPLL_MODE_PHASEBOX, 1, 0);
    shw_pps_gen_enable_output(1);

    while(!spll_check_lock(0))
    {
        pp_printf(".");
	    timer_delay_ms(200);
        n++;

        if( n == timeout )
        {
            pp_printf("\nERROR: can't lock PLL.\n");
            return 0;
        }
    }

    pp_printf("\n");

	spll_phasebox_start_ptracker(ref, chan);
    timer_delay_ms(200);

	for (i = 0; i < n_samples; )
	{
		int p1;

		if (spll_phasebox_read_ptracker(chan, &p1))
		{
			pp_printf("PHASE %d\n", p1);
            i++;
		}
	}

	return 0;
}

DEFINE_WRC_COMMAND(measure) = {
	.name = "measure",
	.exec = cmd_measure,
};
