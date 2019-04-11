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
#include "ptpd_netif.h"

extern int ext_pps_latency_ps=4000;

static int cmd_calibration(const char *args[])
{
	uint32_t trans[wr_num_ports];
	int port = 0;
	int ret=0;

	if (args[0] && !strcasecmp(args[0], "force")) {
		for (port = 0; port < wr_num_ports; port++) {
			if (link_status[port] == LINK_DOWN) continue;
			pp_printf("Port %d Measuring t2/t4 phase transition...\n", port);
			if (measure_t24p(&trans[port], port) < 0)
				ret = -1;
			else
			{
				ret = storage_phtrans(&trans[port], 1, port);
			}
		}
		return ret;
	} else if (args[0] && !strcasecmp(args[0], "ext")) {
		ext_pps_latency_ps = atoi(args[1]);
	}
	else {
		for (port = 0; port < wr_num_ports; port++) {
			if (storage_phtrans(&trans[port], 0, port) > 0) {
				pp_printf("Port %d Found phase transition in EEPROM: %dps\n",
					port, trans[port]);
				cal_phase_transition[port] = trans[port];
			} else {
				if (link_status[port] == LINK_DOWN) continue;
				pp_printf("Port %d Measuring t2/t4 phase transition...\n", port);
				if (measure_t24p(&trans[port], port) < 0)
					ret =-1;
				else {
					cal_phase_transition[port] = trans[port];
					ret = storage_phtrans(&trans[port], 1, port);
				}
			}
		}
		pp_printf("Ext PPS input delay %d\n", ext_pps_latency_ps);
	}
	return ret;
}

DEFINE_WRC_COMMAND(calibration) = {
	.name = "calibration",
	.exec = cmd_calibration,
};
