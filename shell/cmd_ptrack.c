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
#ifdef CONFIG_WRPC_PPSI
#  include <ppsi/ppsi.h>
#  include "wr-api.h"
#else
#  include "ptpd.h"
#endif

extern int wrc_phase_tracking;
extern struct pp_globals *ppg;

char *track_label[] = {
	"OFF",
	"ON",
#ifdef CONFIG_INSITU_CALIB
	"insitu (synthonization only)",
#endif
};

static int cmd_ptrack(const char *args[])
{
	struct pp_instance *ppi = ppg->pp_instances;

#ifdef CONFIG_HAS_EXT_WR
	if (args[0] && !strcasecmp(args[0], "enable")) {
		wrh_servo_enable_tracking(1);
	}
	else if (args[0] && !strcasecmp(args[0], "disable")) {
		wrh_servo_enable_tracking(0);
	}
	else if (HAS_INSITU_CALIB
		 && args[0] && !strcasecmp(args[0], "insitu")) {
		wrh_servo_enable_tracking(2);
	}

	if (ppi->protocol_extension==PPSI_EXT_WR && ppi->extState==PP_EXSTATE_ACTIVE)
		pp_printf("phase tracking %s\n", track_label[WRH_SRV(ppi)->tracking_enabled]);
#endif

#if CONFIG_HAS_EXT_L1SYNC
	pp_printf("phase tracking not implemented for L1sync!\n");
#endif

	return 0;
}

DEFINE_WRC_COMMAND(ptrack) = {
	.name = "ptrack",
	.exec = cmd_ptrack,
};
