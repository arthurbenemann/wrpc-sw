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
int32_t sfp_alpha = 73622176; /* default values if could not read EEPROM */
int32_t sfp_deltaTx = 0;
int32_t sfp_deltaRx = 0;
int32_t sfp_in_db = 0;

char sfp_pn[SFP_PN_LEN];

int sfp_present(void)
{
	return !gpio_in(GPIO_SFP_DET);
}

/*
 * Select channel 1 on the PCA9548
 */
void pca9548_select() {

    mi2c_init(WRPC_FMC_I2C);

    mi2c_start(WRPC_FMC_I2C);
    mi2c_put_byte(WRPC_FMC_I2C, (FMC_PCA9548_ADR << 1));
    mi2c_put_byte(WRPC_FMC_I2C, (1 << 1));
    mi2c_stop(WRPC_FMC_I2C);

}

int sfp_read_user(int *user_space)
{
	int i;
	int value;
	uint8_t data;
	mi2c_init(WRPC_SFP_I2C);

/*	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2); // Page A2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f); // 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, 0x00); // page 0x00
	mi2c_stop(WRPC_SFP_I2C);

*/	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2); // Page A2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x80); // Address 0x80
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3); // Page A2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	value = data;
//	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
//	value = (value<<8) + data;
	mi2c_stop(WRPC_SFP_I2C);

	*user_space = value;

	return -1;
}

int sfp_dump_a2(char *a2)
{
	int i;
	uint8_t data;
	mi2c_init(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2); // Page A2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x00); // Address 0x00
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3); // Page A2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	a2[0] = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3);
	for (i = 1; i < 255; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		a2[i] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);
	a2[255] = data;

	return -1;
}

int sfp_write_user(int *user_space)
{
	uint8_t msb,lsb;
	mi2c_init(WRPC_SFP_I2C);

	msb = (*user_space & 0xff00)>>8;
	lsb = *user_space & 0xff;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2); // Page A2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f); // 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, 0x00); // page 0x00
	mi2c_stop(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2); // Page A2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x80); // 0x80 => USER_SPACE
	mi2c_put_byte(WRPC_SFP_I2C, lsb);
	mi2c_stop(WRPC_SFP_I2C);

	return -1;
}

int sfp_read_laser_wavelength(int *laser_wavelength)
{
	int i;
	int value;
	uint8_t data, sum;
	mi2c_init(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA0);
	mi2c_put_byte(WRPC_SFP_I2C, 0x00);
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	sum = data;
	value = 0;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	for (i = 1; i < 63; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		sum = (uint8_t) ((uint16_t) sum + data) & 0xff;
		if (i >= 60 && i <= 61)	//Laser Wavelength
			value = (value<<8) + data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);

	*laser_wavelength = value;

	if (sum == data)
		return 0;

	return -1;
}

int sfp_read_part_id(char *part_id)
{
	int i;
	uint8_t data, sum;
	mi2c_init(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA0);
	mi2c_put_byte(WRPC_SFP_I2C, 0x00);
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	sum = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	for (i = 1; i < 63; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		sum = (uint8_t) ((uint16_t) sum + data) & 0xff;
		if (i >= 40 && i <= 55)	//Part Number
			part_id[i - 40] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);

	if (sum == data)
		return 0;

	return -1;
}

int sfp_match(void)
{
	struct s_sfpinfo sfp;

	sfp_pn[0] = '\0';
	if (!sfp_present()) {
		return -ENODEV;
	}
	if (sfp_read_part_id(sfp_pn)) {
		return -EIO;
	}

	strncpy(sfp.pn, sfp_pn, SFP_PN_LEN);
	if (storage_match_sfp(&sfp) == 0) {
		sfp_in_db = SFP_NOT_MATCHED;
		return -ENXIO;
	}
	sfp_deltaTx = sfp.dTx;
	sfp_deltaRx = sfp.dRx;
	sfp_alpha = sfp.alpha;
	sfp_in_db = SFP_MATCHED;
	return 0;
}
