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

			pp_printf("Setting calibration parameter %s [0x%x] to %d\n",
				  args[1], (unsigned int) param, value);
			storage_set_calibration_parameter_and_save( param, value );
		}
#ifdef CONFIG_CMD_CALIBRATION_SHOW
		else if (!strcmp(args[0], "show")) {
			wrc_cal_data_t *cal;
			unsigned i;

			if (!storage_is_calibration_loaded()) {
				pp_printf("calibrations not loaded\n");
				return 0;
			}
			cal = storage_get_calibration_data();
			pp_printf("%u params:\n", (unsigned)cal->param_count);
			for(i = 0; i < cal->param_count; i++) {
				unsigned id = cal->params[i].id;
				pp_printf( " %c%c%c%c = %u\n",
					   (id >> 24) & 0xff,
					   (id >> 16) & 0xff,
					   (id >> 8) & 0xff,
					   (id >> 0) & 0xff,
					   (unsigned)cal->params[i].value);
			}
		}
#endif
	} else if (!args[0]) {
		if (storage_phtrans(&trans, 0) > 0) {
			pp_printf("Found phase transition in EEPROM: %dps\n",
				  (unsigned int) trans);

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
