/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2023-2024 Missing Link Electronics (www.missinglinkelectronics.com)
 *                         CERN (www.cern.ch)
 * Author: Frederik Pfautsch <frederik.pfautsch@missinglinkelectronics.com>
 *         Oskar Szakinnis <oskar.szakinnis@missinglinkelectronics.com>
 *         (based on work by Greg Daniluk <grzegorz.daniluk@cern.ch>)
 *
 * This file is intended to cover a range of AMD development boards.
 * Currently, the following boards are supported:
 *   - ZCU102
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <stdbool.h>
#include <string.h>

#include "board.h"
#include "dev/bb_i2c.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "storage.h"
#include "wrc-debug.h"
#include "wrc_global.h"

typedef enum {
	ZCU102,
	/* additional boards can be added here */
	NUM_SUPPORTED_BOARDS,
	UNSUPPORTED,
} supported_boards_t;

typedef struct {
	uint8_t addr;
	uint8_t ch_bitmask;
} i2c_mux_cfg_t;

static struct i2c_bus i2c_wrc_general;
static struct i2c_bus i2c_wrc_eeprom;
static struct i2c_eeprom_device wrc_eeprom_mac_dev;
static struct i2c_eeprom_device wrc_eeprom_sdbfs_dev;

static supported_boards_t this_board = UNSUPPORTED;

/* match board type with hardware name string in syscon */
static const char * const supported_boards_hw_name_strs[NUM_SUPPORTED_BOARDS] = {
	[ZCU102] = "X102"
};

/* board names for printing */
static const char * const supported_boards_strs[NUM_SUPPORTED_BOARDS] = {
	[ZCU102] = "ZCU102"
};

/* I2C MUX configurations per board */
static const i2c_mux_cfg_t zcu102_i2c_mux_cfg[] = {
	/* Mux 0 */
	{.addr = ZCU102_I2C_MUX0_ADR,
	 .ch_bitmask = ZCU102_I2C_MUX0_CH_BIT_EEPROM
	             | ZCU102_I2C_MUX0_CH_BIT_SI5341
	             | ZCU102_I2C_MUX0_CH_BIT_SI570},
	/* Mux 1 */
	{.addr = ZCU102_I2C_MUX1_ADR,
	 .ch_bitmask = ZCU102_I2C_MUX1_CH_BIT_SFP0}
};

/*
 * due to being static, any member that is not explicitly defined will default
 * to 0, so check for nullptr before trying to configure MUX
 */
static const i2c_mux_cfg_t *i2c_mux_cfgs[NUM_SUPPORTED_BOARDS] = {
	[ZCU102] = zcu102_i2c_mux_cfg
};

/*
 * generic I2C MUX configuration, assuming the process of enabling channels
 * consists of simply writing a bitmask to some I2C address.
 */
void generic_i2c_mux_apply_cfg(struct i2c_bus *i2c_bus, const i2c_mux_cfg_t *i2c_mux_cfg) {
	bb_i2c_start(i2c_bus);
	bb_i2c_put_byte(i2c_bus, i2c_mux_cfg->addr << 1);
	bb_i2c_put_byte(i2c_bus, i2c_mux_cfg->ch_bitmask);
	bb_i2c_stop(i2c_bus);
}

int wrc_board_early_init()
{
	/*
	 * fetch specific board type from SYSCON
	 * TODO: could initialization of wrc_global_link.wrc_hw_name be moved before
	 * wrc_board_early_init() in wrc_main.c?
	 */
	char hw_name[5] = {0};
	get_hw_name(hw_name);
	for (int i = 0; i < NUM_SUPPORTED_BOARDS; i++) {
		if (strncmp(supported_boards_hw_name_strs[i], hw_name, 4) == 0) {
			this_board = i;
			board_dbg("AMD development board: %s\n", supported_boards_strs[this_board]);
		}
	}
	if (this_board == UNSUPPORTED) {
		board_dbg("WARNING: Not a supported AMD development board (hardware name: %s). Attempting generic initialization, but functionality may be missing or broken.\n", hw_name);
	}

	/*
	 * create and init I2C busses.
	 */
	bb_i2c_create(&i2c_wrc_general, &pin_sysc_sfp1_scl, &pin_sysc_sfp1_sda);
	bb_i2c_init(&i2c_wrc_general);

	bb_i2c_create(&i2c_wrc_eeprom, &pin_sysc_fmc_scl, &pin_sysc_fmc_sda);
	bb_i2c_init(&i2c_wrc_eeprom);

	/*
	 * For all currently supported boards that utilize one or more I2C Muxes,
	 * it's sufficient to enable all relevant channels on the MUX, as there is
	 * no address overlap that would require using only one channel at a time.
	 */
	if (i2c_mux_cfgs[this_board]) {
		int n = sizeof(i2c_mux_cfgs[this_board])/sizeof(i2c_mux_cfg_t);
		for (int i = 0; i < n; i++) {
			generic_i2c_mux_apply_cfg(&i2c_wrc_general, &i2c_mux_cfgs[this_board][i]);
		}
	}

	/*
	 * Set up EEPROM(s) and SDBFS
	 * Notice:
	 *   - ZCU10x family devices may have board-level metadata stored in the
	 *     'regular' EEPROM (including a MAC address, which will be used if the
	 *     SDBFS doesn't contain one). In order not to corrupt the data present
	 *     in the "regular" EEPROM, SDBFS is stored in HDMI EDID EEPROM instead.
	 */

	/* default to M24C08 EEPROM */
	uint8_t eeprom_i2c_addr = EEPROM_M24C08_ADR;
	int eeprom_offset_bytes = EEPROM_M24C08_BYTE_OFFSET;
	if (this_board == ZCU102) {
		/* for ZCU102, use HDMI EDID EEPROM instead*/
		eeprom_i2c_addr = EEPROM_HDMI_EDID_ADR;
		eeprom_offset_bytes = EEPROM_HDMI_EDID_BYTE_OFFSET;
	}
	i2c_eeprom_create(&wrc_eeprom_sdbfs_dev, &i2c_wrc_eeprom, eeprom_i2c_addr, eeprom_offset_bytes);
	storage_i2ceeprom_create(&wrc_storage_dev, &wrc_eeprom_sdbfs_dev);
	storage_mount(&wrc_storage_dev);

	/* create MAC EEPROM for ZCU10X */
	if (this_board == ZCU102) {
		i2c_eeprom_create(&wrc_eeprom_mac_dev, &i2c_wrc_general, EEPROM_M24C08_ADR, EEPROM_M24C08_BYTE_OFFSET);
	}

	return 0;
}

int wrc_board_init()
{
	uint8_t mac_addr[6];

	if (storage_get_persistent_mac(0, mac_addr) == 0) {
		board_dbg("Got MAC address from SDBFS\n");
	} else if (this_board == ZCU102 &&
	           i2c_eeprom_read(&wrc_eeprom_mac_dev, 0x20, mac_addr, 6) == 6) {
		board_dbg("Got MAC address from board-level metadata EEPROM\n");
	} else {
		board_dbg("Failed to get MAC address from EEPROM. Using fallback address.\n");
		mac_addr[0] = 0x22;
		mac_addr[1] = 0x33;
		mac_addr[2] = 0x44;
		mac_addr[3] = 0x55;
		mac_addr[4] = 0x66;
		mac_addr[5] = 0x77;
	}
	ep_set_mac_addr(&wrc_endpoint_dev, mac_addr);
	ep_pfilter_init_default(&wrc_endpoint_dev);

	return 0;
}

int wrc_board_create_tasks()
{
	return 0;
}
