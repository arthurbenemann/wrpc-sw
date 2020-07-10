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
		Arguments: [force]

		Description: launches RX timestamper calibration. */

#include <string.h>
#include <wrc.h>
#include "shell.h"
#include "storage.h"
#include "dev/syscon.h"
#include "dev/rxts_calibrator.h"

static int cmd_calibration(const char *args[])
{
	uint32_t trans;

	if (args[0])
	{
		if(!strcasecmp(args[0], "force")) 
		{
			if (measure_t24p(&trans) < 0)
				return -1;
			return storage_phtrans(&trans, 1);
		}
		else if (!strcasecmp( args[0], "load" ) )
		{
			storage_load_calibration();
		}
		else if (!strcasecmp( args[0], "setp" ) )
		{
			uint32_t param = 0;
			param |= ((uint32_t)(args[1][0])) << 24;
			param |= ((uint32_t)(args[1][1])) << 16;
			param |= ((uint32_t)(args[1][2])) << 8;
			param |= ((uint32_t)(args[1][3])) << 0;

			int value = atoi(args[2]);

			pp_printf("Setting calibration parameter %s [0x%x] to %d\n", args[1], param ,value );
			storage_set_calibration_parameter( param, value );
		}

	} else if (!args[0]) {
		if (storage_phtrans(&trans, 0) > 0) {
			pp_printf("Found phase transition in EEPROM: %dps\n",
				trans);

			return 0;
		} else {
			pp_printf("Measuring t2/t4 phase transition...\n");
			if (measure_t24p(&trans) < 0)
				return -1;

			return storage_phtrans(&trans, 1);
		}
	}

	return 0;
}

DEFINE_WRC_COMMAND(calibration) = {
	.name = "calibration",
	.exec = cmd_calibration,
};
