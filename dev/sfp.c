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

static int sfp_present(void)
{
	return !gpio_in(GPIO_SFP_DET);
}

/*
 * Select channel 1 on the PCA9548
 */
/*
 void pca9548_select() {

    mi2c_init(WRPC_FMC_I2C);

    mi2c_start(WRPC_FMC_I2C);
    mi2c_put_byte(WRPC_FMC_I2C, (FMC_PCA9548_ADR << 1));
    mi2c_put_byte(WRPC_FMC_I2C, (1 << 1));
    mi2c_stop(WRPC_FMC_I2C);

}
*/
int sfp_sel_page2(void)
{
	mi2c_init(WRPC_SFP_I2C);

	// Select A2 Page
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);		// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f);		// 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, 0x02);		// page 0x02
	mi2c_stop(WRPC_SFP_I2C);

	return -1;
}

int sfp_rd(uint8_t i2c_addr, uint8_t addr, uint8_t page, uint8_t *value)
{
	uint8_t data, i2c_rd_addr;
	mi2c_init(WRPC_SFP_I2C);

	// Select A2 Page
        // (note: for unknown reason page must be selected even when
        //  memory locations 0-127 are addressed or 0xA0 is addressed)
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);		// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f);		// 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, page);		// page
	mi2c_stop(WRPC_SFP_I2C);

	i2c_rd_addr = i2c_addr + 1;

	// Start reading Lower Memory	
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_addr);		// 0xA0 or 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x00);
	// Repeated Start reading page Memory
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_addr);		// 0xA0 or 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, addr);		// page => addr
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_rd_addr);	// 0xA0 or 0xA2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	*value = data;

	return -1;
}

int sfp_wr(uint8_t i2c_addr, uint8_t addr, uint8_t page, uint8_t value)
{
	mi2c_init(WRPC_SFP_I2C);

	pp_printf("sfpwr i2c_addr: 0x%02x, addr: 0x%02x, page: 0x%02x, data: 0x%02x\n", i2c_addr, addr, page, value);


	// Select A2 Page
        // (note: for unknown reason page must be selected even when
        //  memory locations 0-127 are addressed or 0xA0 is addressed)
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);		// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f);		// 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, page);		// page
	mi2c_stop(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_addr);		// 0xA0 or 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, addr);		// addr
	mi2c_put_byte(WRPC_SFP_I2C, value);		// write value
	mi2c_stop(WRPC_SFP_I2C);

	return -1;
}

int sfp_dump(char *memdump, uint8_t i2c_addr, uint8_t page)
{
	int i;
	uint8_t data, i2c_rd_addr;
	mi2c_init(WRPC_SFP_I2C);

	if (i2c_addr == 0xA2) {
		// Select A2 Page
		pp_printf("SFP memory dump of i2c addr: 0x%02x page: 0x%02x\n",i2c_addr, page);
		mi2c_start(WRPC_SFP_I2C);
		mi2c_put_byte(WRPC_SFP_I2C, 0xA2);	// 0xA2 + Write
		mi2c_put_byte(WRPC_SFP_I2C, 0x7f);	// 0x7f => Table select
		mi2c_put_byte(WRPC_SFP_I2C, page);	// page
		mi2c_stop(WRPC_SFP_I2C);
	} else {
		pp_printf("SFP memory dump of i2c addr: 0x%02x\n",i2c_addr);
	}
	i2c_rd_addr = i2c_addr + 1;

	// Start reading a sequence of 128 bytes in Lower/Expanded Memory	
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_addr);		// 0xA0 or 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x00);		// Address 0x00
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_rd_addr);	// 0xA0 or 0xA2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	memdump[0] = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_rd_addr);	// 0xA0 or 0xA2 + Read
	for (i = 1; i < 127; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		memdump[i] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);		//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);
	memdump[127] = data;

	// For some reason, if you read from 0x00 to 0xff in one go then
	// 0x00-0x7f are read twice instead of reading 0x00-0xff.
	// Restart reading a sequence of 128 bytes in A2 Upper Memory	
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_addr);		// 0xA0 or 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x80);		// Address 0x80
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, i2c_rd_addr);	// 0xA0 or 0xA2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	memdump[128] = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3);
	for (i = 129; i < 255; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		memdump[i] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);
	memdump[255] = data;
	return -1;
}

int sfp_wr_ch(uint8_t ch)
{
	if ((ch == 0) || (ch >= 103)) {
		pp_printf("channel %d out of range [1..102]\n", ch);
 		return 0;
	} else {
		mi2c_init(WRPC_SFP_I2C);

		pp_printf("write channel: %d\n", ch);

/*
		// Select A2 Page 0x02
		mi2c_start(WRPC_SFP_I2C);
		mi2c_put_byte(WRPC_SFP_I2C, 0xA2);	// 0xA2 + Write
		mi2c_put_byte(WRPC_SFP_I2C, 0x7f);	// 0x7f => Table select
		mi2c_put_byte(WRPC_SFP_I2C, 0x02);	// page 0x02
		mi2c_stop(WRPC_SFP_I2C);
*/
		mi2c_start(WRPC_SFP_I2C);
		mi2c_put_byte(WRPC_SFP_I2C, 0xA2);	// 0xA2 + Write
		mi2c_put_byte(WRPC_SFP_I2C, 0x90);	// addr 144: Channel Number Set
		mi2c_put_byte(WRPC_SFP_I2C, 0x00);	// 0x00
		mi2c_put_byte(WRPC_SFP_I2C, ch);	// channel
		mi2c_stop(WRPC_SFP_I2C);

		return -1;
	}
}

int sfp_rd_ch(uint8_t *ch_number, int *ch_wl)
{
	uint8_t data;
	int value; 
	mi2c_init(WRPC_SFP_I2C);

/*
	// Select A2 Page
        // (note: for unknown reason page must be selected even when
        //  memory locations 0-127 are addressed or 0xA0 is addressed)
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);		// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f);		// 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, 0x02);		// page 0x02
	mi2c_stop(WRPC_SFP_I2C);
*/

	// Repeated Start reading page Memory
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);	// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x90);	// addr 144: Channel Number Set
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3);	// 0xA2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
        *ch_number = data;
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
        value = (data << 8);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
        value = value + data;
	mi2c_stop(WRPC_SFP_I2C);
	*ch_wl = value;

	return -1;
}

int sfp_rd_ch_stat(uint8_t *stat)
{
	uint8_t data;
	mi2c_init(WRPC_SFP_I2C);

/*
	// Select A2 Page
        // (note: for unknown reason page must be selected even when
        //  memory locations 0-127 are addressed or 0xA0 is addressed)
	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);		// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0x7f);		// 0x7f => Table select
	mi2c_put_byte(WRPC_SFP_I2C, 0x02);		// page 0x02
	mi2c_stop(WRPC_SFP_I2C);
*/
	// Repeated Start reading page Memory
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA2);	// 0xA2 + Write
	mi2c_put_byte(WRPC_SFP_I2C, 0xA8);	// addr 144: Channel Number Set
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA3);	// 0xA2 + Read
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);
	*stat = data;
	pp_printf("Status: 0x%02x\n", data);
	if (data && 0x40) pp_printf("TEC Fault\n");
	if (data && 0x20) pp_printf("Wavelength Unlocked Condition\n");
	if (data && 0x10) pp_printf("Tx not ready due to tuning\n");

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

static int sfp_read_part_id(char *part_id)
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
