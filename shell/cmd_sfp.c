/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/*  Command: sfp
    Arguments: subcommand [subcommand-specific args]

    Description: SFP detection/database manipulation.

		Subcommands:
			add vendor_type delta_tx delta_rx alpha - adds an SFP to the database, with given alpha/delta_rx/delta_rx values
			show - shows the SFP database
      match - tries to get calibration parameters from DB for a detected SFP
      erase - cleans the SFP database
			detect - detects the transceiver type
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
	int i, j;
	int8_t sfpcount = 1, temp;
	int laser_wavelength, aap;
	uint8_t ch_number, stat;
	int ch_wavelength;
	uint8_t i2c_addr;
	int data;
	uint8_t value;
	static char memdump[256] = "\0";
//	static char line_8[8] = "\0";
	struct s_sfpinfo sfp;
	static char pn[SFP_PN_LEN + 1] = "\0";

	if (args[0] && !strcasecmp(args[0], "detect")) {
		if (!sfp_present())
			pp_printf("No SFP.\n");
		else
			sfp_read_part_id(pn);
		pn[16] = 0;
		pp_printf("%s\n", pn);
		return 0;
	}
//  else if (!strcasecmp(args[0], "i2cscan"))
//  {
//    mi2c_scan(WRPC_FMC_I2C);
//    return 0;
//  }
	else if (!strcasecmp(args[0], "erase")) {
		if (storage_sfpdb_erase() ==
		    EE_RET_I2CERR)
			pp_printf("Could not erase DB\n");
	} else if (args[4] && !strcasecmp(args[0], "add")) {
		if (strlen(args[1]) > 16)
			temp = 16;
		else
			temp = strlen(args[1]);
		for (i = 0; i < temp; ++i)
			sfp.pn[i] = args[1][i];
		while (i < 16)
			sfp.pn[i++] = ' ';	//padding
		sfp.dTx = atoi(args[2]);
		sfp.dRx = atoi(args[3]);
		sfp.alpha = atoi(args[4]);
		temp = storage_get_sfp(&sfp, 1, 0);
		if (temp == EE_RET_DBFULL)
			pp_printf("SFP DB is full\n");
		else if (temp == EE_RET_I2CERR)
			pp_printf("I2C error\n");
		else
			pp_printf("%d SFPs in DB\n", temp);
	} else if (args[0] && !strcasecmp(args[0], "show")) {
		for (i = 0; i < sfpcount; ++i) {
			temp = storage_get_sfp(&sfp, 0, i);
			if (!i) {
				sfpcount = temp;	//only in first round valid sfpcount is returned from storage_get_sfp
				if (sfpcount == 0 || sfpcount == 0xFF) {
					pp_printf("SFP database empty...\n");
					return 0;
				} else if (sfpcount == -1) {
					pp_printf("SFP database corrupted...\n");
					return 0;
				}
			}
			pp_printf("%d: PN:", i + 1);
			for (temp = 0; temp < 16; ++temp)
				pp_printf("%c", sfp.pn[temp]);
			pp_printf(" dTx: %d, dRx: %d, alpha: %d\n", sfp.dTx,
				sfp.dRx, sfp.alpha);
		}
	} else if (args[0] && !strcasecmp(args[0], "match")) {
		if (pn[0] == '\0') {
			pp_printf("Run sfp detect first\n");
			return 0;
		}
		strncpy(sfp.pn, pn, SFP_PN_LEN);
		if (storage_match_sfp(&sfp) > 0) {
			pp_printf("SFP matched, dTx=%d, dRx=%d, alpha=%d\n",
				sfp.dTx, sfp.dRx, sfp.alpha);
			sfp_deltaTx = sfp.dTx;
			sfp_deltaRx = sfp.dRx;
			sfp_alpha = sfp.alpha;
		} else
			pp_printf("Could not match to DB\n");
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "rd_wl")) {
		pp_printf("Set SFP TX-Laser wavelength to Channel 11\n");
		sfp_read_laser_wavelength(&laser_wavelength);
		pp_printf("Laser wavelength: %d\n", laser_wavelength);

		//pp_printf("write user_space...\n");
		//aap = 101;		
		//sfp_write_user(aap);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "rd")) {
		if (!strcasecmp(args[1], "a2")) {
			i2c_addr = 0xa2;
		} else {
			i2c_addr = 0xa0;
		}
		sfp_rd(i2c_addr, atoi(args[2]), atoi(args[3]), &value);
                pp_printf("i2c_addr: 0x%02x, addr: 0x%02x, page: 0x%02x, data: 0x%02x\n", i2c_addr, atoi(args[2]), atoi(args[3]), value);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "wr")) {
		if (!strcasecmp(args[1], "a2")) {
			i2c_addr = 0xa2;
		} else {
			i2c_addr = 0xa0;
		}
		value = atoi(args[4]) & 0xff;
                pp_printf("writing:  i2c_addr: 0x%02x, addr: 0x%02x, page: 0x%02x, data: 0x%02x\n", i2c_addr, atoi(args[2]), atoi(args[3]), value);
		sfp_wr(i2c_addr, atoi(args[2]), atoi(args[3]), value);
		sfp_rd(i2c_addr, atoi(args[2]), atoi(args[3]), &value);
                pp_printf("readback: i2c_addr: 0x%02x, addr: 0x%02x, page: 0x%02x, data: 0x%02x\n", i2c_addr, atoi(args[2]), atoi(args[3]), value);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "sel_page2")) {
		sfp_sel_page2();
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "wr_ch")) {
		sfp_sel_page2();
		sfp_wr_ch(atoi(args[1]));
		sfp_rd_ch_stat(&stat);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "rd_ch")) {
		sfp_rd_ch(&ch_number, &ch_wavelength);
                pp_printf("Read Channel Number %d, wavelength 0x%02x\n",ch_number, ch_wavelength);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "rd_ch_stat")) {
		sfp_rd_ch_stat(&stat);
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "dump")) {
		if (!strcasecmp(args[1], "a2")) {
			i2c_addr = 0xa2;
		} else {
			i2c_addr = 0xa0;
		}
		sfp_dump(memdump, i2c_addr, atoi(args[2]));
		j = 0;
		for (i = 0; i < 256; ++i) {
			if (j== 0) {
				pp_printf("%02x: ", i);
			}			
			pp_printf("%02x ", memdump[i]);
 			j++;
			if (j == 8) {
				pp_printf("\n");
				j = 0;
			}
		}
		pp_printf("\n");
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "ch32")) {
		pp_printf("Set SFP TX-Laser wavelength to Channel 32\n");
		return 0;
	} else if (args[0] && !strcasecmp(args[0], "ena")) {
		if(!args[1])
			return -EINVAL;
		ep_sfp_enable(atoi(args[1]));
	}

	return 0;
}

DEFINE_WRC_COMMAND(sfp) = {
	.name = "sfp",
	.exec = cmd_sfp,
};
