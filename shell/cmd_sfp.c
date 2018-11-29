/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/* Command: sfp
 * Arguments: subcommand [subcommand-specific args]
 *
 * Description: SFP detection/database manipulation.
 * Subcommands:
 * add <product_number> <delta_tx> <delta_rx> <alpha> - adds an SFP to
 *                      the database, with given alpha/delta_rx/delta_tx values
 * show - shows the SFP database
 * match - detects the transceiver type and tries to get calibration parameters
 *         from DB for a detected SFP
 * erase - cleans the SFP database
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wrc.h>

#include "shell.h"
#include "storage.h"
#include "syscon.h"
#include "endpoint.h"

#include "sfp.h"

static int cmd_sfp(const char *args[])
{
	int8_t sfpcount[] = {1,1};
	int8_t i, temp, ret=0;
	int port;

	struct s_sfpinfo sfp;

	if (!args[0]) {
		pp_printf("Wrong parameter\n");
		return -EINVAL;
	}
	if (!strcasecmp(args[0], "erase")) {
		if (args[1])
			port = atoi(args[1]);
		else
			port = 0;
		
		if (port > 1) return -EINVAL;
		
		if (storage_sfpdb_erase(port) == EE_RET_I2CERR) {
			pp_printf("Port %d Could not erase DB\n", port);
			ret = -EIO;
		}
		return ret;

	} else if (args[4] && !strcasecmp(args[0], "add")) {
		temp = strnlen(args[1], SFP_PN_LEN);
		for (i = 0; i < temp; ++i)
			sfp.pn[i] = args[1][i];
		while (i < SFP_PN_LEN)
			sfp.pn[i++] = ' ';	//padding
		sfp.dTx = atoi(args[2]);
		sfp.dRx = atoi(args[3]);
		sfp.alpha = atoi(args[4]);

		if (args[5])
			sfp.port = atoi(args[5]);
		else
			sfp.port = 0;

		if (sfp.port > 1) return -EINVAL;
		temp = storage_get_sfp(&sfp, SFP_ADD, 0, sfp.port);
		if (temp == EE_RET_DBFULL) {
			pp_printf("SFP DB is full\n");
			return -ENOSPC;
		} else if (temp == EE_RET_I2CERR) {
			pp_printf("I2C error\n");
			return -EIO;
		} else if (temp < 0) {
			pp_printf("Port %d SFP database error (%d)\n", sfp.port, temp);
			return -EFAULT;
		}
		pp_printf("Port %d has %d SFPs in DB\n", sfp.port, temp);
		return 0;
	} else if (!strcasecmp(args[0], "show")) {
		for (port = 0; port < wr_num_ports; ++port) {
			for (i = 0; i< sfpcount[port]; ++i) {
				sfpcount[port] = storage_get_sfp(&sfp, SFP_GET, i, port);
				if (sfpcount[port] == 0) {
					pp_printf("Port %d SFP database empty\n", port);
				} else if (sfpcount[port] < 0) {
					pp_printf("Port %d SFP database error (%d)\n", port,
						  sfpcount[port]);
					ret = -EFAULT;
				} else {
					pp_printf("Port %d, SFP %d: PN:", port, i + 1);
					for (temp = 0; temp < SFP_PN_LEN; ++temp)
						pp_printf("%c", sfp.pn[temp]);
					pp_printf(" dTx: %8d dRx: %8d alpha: %8d\n", sfp.dTx,
						sfp.dRx, sfp.alpha);
				}
			}
		}
		return ret;
	} else if (!strcasecmp(args[0], "match")) {
		for (port = 0; port < wr_num_ports; ++port) {
			
			ret = sfp_match(port);
			if (ret == -ENODEV) {
				pp_printf("Port %d No SFP.\n", port);
				continue;
			} else if (ret == -EIO) {
				pp_printf("Port %d SFP read error\n", port);
				continue;
			} 

			/* SFP read correctly */
			for (temp = 0; temp < SFP_PN_LEN; ++temp)
				pp_printf("%c", sfp_pn[port][temp]);
			pp_printf("\n");

			if (ret == -ENXIO) {
				pp_printf("Port %d Could not match to DB\n", port);
				continue;
			}

			/* match successful */
			pp_printf("Port %d SFP matched, dTx=%d dRx=%d alpha=%d\n",
				port, sfp_deltaTx[port], sfp_deltaRx[port], sfp_alpha[port]);
		}
	
		return ret;

	} else if (args[1] && !strcasecmp(args[0], "ena")) {
		if (args[2])
			sfp.port = atoi(args[2]);
		else
			sfp.port = 0;

		if (sfp.port > 1) return -EINVAL;

		ep_sfp_enable(atoi(args[1]), sfp.port);
		return 0;
	} else {
		pp_printf("Wrong parameter\n");
		return -EINVAL;
	}
	return 0;
}

DEFINE_WRC_COMMAND(sfp) = {
	.name = "sfp",
	.exec = cmd_sfp,
};
