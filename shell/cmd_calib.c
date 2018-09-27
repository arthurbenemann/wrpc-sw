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
#include "syscon.h"
#include "rxts_calibrator.h"

static int cmd_calibration(const char *args[])
{
	uint32_t trans;
	int port=0;

	if (args[0] && !strcasecmp(args[0], "force")) {
		if (measure_t24p(&trans, port) < 0)
			return -1;
		return storage_phtrans(&trans, 1, port);
	} else if (!args[0]) {
		if (storage_phtrans(&trans, 0, port) > 0) {
			pp_printf("Port %d Found phase transition in EEPROM: %dps\n",
				port, trans);
			cal_phase_transition[port] = trans;
			return 0;
		} else {
			pp_printf("Port %d Measuring t2/t4 phase transition...\n", port);
			if (measure_t24p(&trans, port) < 0)
				return -1;
			cal_phase_transition[port] = trans;
			return storage_phtrans(&trans, 1, port);
		}
	}

	return 0;
}

DEFINE_WRC_COMMAND(calibration) = {
	.name = "calibration",
	.exec = cmd_calibration,
};
