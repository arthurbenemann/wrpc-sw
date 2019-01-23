/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/* SFP Detection / managenent functions */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "syscon.h"
#include "i2c.h"
#include "sfp.h"
#include "storage.h"

/* Calibration data (from EEPROM if available) */
int32_t sfp_alpha[] = {64398396,-64398396}; /* default values if could not read EEPROM */
int32_t sfp_deltaTx[] = {0,0};
int32_t sfp_deltaRx[] = {0,0};
int32_t sfp_in_db[] = {0,0};

char sfp_pn[wr_num_ports][SFP_PN_LEN];

int sfp_present(int port)
{
	return (port) ? (!gpio_in(GPIO_SFP1_DET)) : (!gpio_in(GPIO_SFP_DET));
}

static int sfp_read_part_id(char *part_id, int port)
{
	int i;
	uint8_t data, sum;
	uint8_t sfp_num;

	if (port==0) sfp_num = WRPC_SFP_I2C;
	else sfp_num = WRPC_DP_SFP_I2C;

	mi2c_init(sfp_num);
	mi2c_start(sfp_num);
	mi2c_put_byte(sfp_num, 0xA0);
	mi2c_put_byte(sfp_num, 0x00);
	mi2c_repeat_start(sfp_num);
	mi2c_put_byte(sfp_num, 0xA1);
	mi2c_get_byte(sfp_num, &data, 1);
	mi2c_stop(sfp_num);

	sum = data;

	mi2c_start(sfp_num);
	mi2c_put_byte(sfp_num, 0xA1);
	for (i = 1; i < 63; ++i) {
		mi2c_get_byte(sfp_num, &data, 0);
		sum = (uint8_t) ((uint16_t) sum + data) & 0xff;
		if (i >= 40 && i <= 55)	//Part Number
			part_id[i - 40] = data;
	}
	mi2c_get_byte(sfp_num, &data, 1);	//final word, checksum
	mi2c_stop(sfp_num);

	if (sum == data)
		return 0;

	return -1;
}

int sfp_match(int port)
{
	struct s_sfpinfo sfp;

	sfp_pn[port][0] = '\0';
	if (!sfp_present(port)) {
		return -ENODEV;
	}
	if (sfp_read_part_id(sfp_pn[port], port)) {
		return -EIO;
	}

	strncpy(sfp.pn, sfp_pn[port], SFP_PN_LEN);
	if (storage_match_sfp(&sfp,port) == 1) {
		if (sfp.port==port)
		{
			sfp_deltaTx[port] = sfp.dTx;
			sfp_deltaRx[port] = sfp.dRx;
			sfp_alpha[port] = sfp.alpha;
			sfp_in_db[port] = SFP_MATCHED;
			pp_printf("port %d SFP matched!\n",port);
			return 0;
		}
	}
	sfp_in_db[port] = SFP_NOT_MATCHED;
	pp_printf("port %d SFP not matched!\n",port);
	return -ENXIO;	
}
