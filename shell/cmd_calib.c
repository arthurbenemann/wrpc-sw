/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* 	Command: calibration
		Arguments: [show] [erase] [t24p]

		Description: launches RX timestamper calibration. */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "storage.h"
#include "syscon.h"
#include "rxts_calibrator.h"

static int cmd_calibration(const char *args[])
{
	uint32_t trans;
	wrc_cal_data_t* cal = storage_get_calibration_data();

	if (!args[0])
		return -1;

	if (!strcasecmp(args[0], "t24p")) {
		pp_printf("Measuring t2/t4 phase transition...\n");
			if (measure_t24p(&trans) < 0)
			return -1;
		storage_set_calibration_parameter(CAL_PARAM_T24P, trans);
		return storage_save_calibration();
	} else if (!strcasecmp(args[0], "show")) {
		int i;

		pp_printf("Calibration parameters: \n");
		for(i = 0; i < cal->param_count; i++)
		{
			pp_printf("  0x%08x: %d\n", cal->params[i].id, cal->params[i].value );
		}
	} else if (!strcasecmp(args[0], "erase")) {
		pp_printf("Erasing calibration parameters.\n");
		cal->param_count = 0;
		storage_save_calibration();
	};

	return 0;
}

DEFINE_WRC_COMMAND(calibration) = {
	.name = "calibration",
	.exec = cmd_calibration,
};
