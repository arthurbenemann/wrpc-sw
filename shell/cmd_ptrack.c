/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#ifdef CONFIG_PPSI
#  include <ppsi/ppsi.h>
#  include "wr-api.h"
#else
#  include "ptpd.h"
#endif

extern int wrc_phase_tracking;

static int cmd_ptrack(const char *args[])
{
	if (args[0] && !strcasecmp(args[0], "enable")) {
		pp_printf("UnFreezing SPLL phase shifter\n");
		spll_vco_freeze(0);
		spll_pshifter_freeze(0);
	}
	else if (args[0] && !strcasecmp(args[0], "ps-freeze")) {
		if( args[1] )
		{
			pp_printf("Freezing SPLL phase shifter at phase %d ps\n", atoi(args[1]));
			spll_set_phase_shift(0, atoi(args[1]) );
			spll_pshifter_freeze(1);
		}
		else
		{
			pp_printf("Freezing SPLL phase shifter");
			spll_pshifter_freeze(1);
		}
		
	}
	else if (args[0] && !strcasecmp(args[0], "vco-freeze")) 
	{
		pp_printf("Freezing SPLL VCO control");
		spll_vco_freeze(1);
	}
	
	return 0;
}

DEFINE_WRC_COMMAND(ptrack) = {
	.name = "ptrack",
	.exec = cmd_ptrack,
};
