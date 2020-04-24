/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 - 2015 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __STORAGE_H
#define __STORAGE_H

#include "sfp.h"

// calibration parameter definitions. Board-specific.
#define CAL_MAX_PARAMS 8
#define CAL_FILE_MAGIC 0xcafebabe

#define ASCII_TO_U32(a, b, c, d) ((((uint32_t)(a)&0xff) << 24) |     \
									 (((uint32_t)(b)&0xff) << 16) | \
									 (((uint32_t)(c)&0xff) << 8) |  \
									 (((uint32_t)(d)&0xff) << 0))

#define CAL_PARAM_T24P ASCII_TO_U32('t', '2', '4', 'p')
#define CAL_PARAM_PHY_TARGET_TX_PHASE ASCII_TO_U32('l', 'p', 't', 'p')
#define CAL_PARAM_DDS_LO_IOUPDATE_DELAY_PS ASCII_TO_U32('e', '1', '4', '0')	
#define CAL_PARAM_DDS_REF_IOUPDATE_DELAY_PS ASCII_TO_U32('e', '1', '4', '1')
#define CAL_PARAM_CLKA_SYNC_DELAY_PS ASCII_TO_U32('e', '1', '4', '2')
#define CAL_PARAM_CLKB_SYNC_DELAY_PS ASCII_TO_U32('e', '1', '4', '3')

#define SFP_SECTION_PATTERN 0xdeadbeef

#if defined CONFIG_LEGACY_EEPROM

#define EE_BASE_CAL (4 * 1024)
#define EE_BASE_SFP (4 * 1024 + 4)
/* Limit SFPs to 3, see comments below why. */
#define SFPS_MAX 3
/* The definition of EE_BASE_INIT below is wrong! But kept for backward
 * compatibility. */
#define EE_BASE_INIT (4 * 1024 + 4 * 29)
/* It should be:
 * #define EE_BASE_INIT (EE_BASE_SFP + sizeof(sfpcount) + \
 *                       SFPS_MAX * sizeof(struct s_sfpinfo))
 * The used definition define the start of the init script 5 bytes
 * (sizeof(sfpcount) + sizeof(t24p)) before the end of SFP database.
 * To make the init script working during the update of old versions of wrpc
 * SFPS_MAX is limited to 3. Adding the 4th SFP will corrupt the init script
 * anyway.
 * If someone needs to have 4 SFPs in the database SFPS_MAX can be set to 4 and
 * the proper define should be used.
 * SDB is not affected by this bug.
 */
#endif

#if defined CONFIG_SDB_STORAGE
#define SFPS_MAX 4
#endif


#define EE_RET_I2CERR -1
#define EE_RET_DBFULL -2
#define EE_RET_CORRPT -3
#define EE_RET_POSERR -4

#ifdef CONFIG_GENSDBFS
#define HAS_GENSDBFS 1
#else
#define HAS_GENSDBFS 0
#endif


struct storage_device;

struct s_sfpinfo {
	char pn[SFP_PN_LEN];
	int32_t alpha;
	int32_t dTx;
	int32_t dRx;
	uint8_t chksum;
} __attribute__ ((__packed__));

typedef struct
{
	uint32_t magic;
	uint32_t param_count;
	uint32_t checksum;
	struct
	{
		uint32_t id;
		uint32_t value;
	} params[CAL_MAX_PARAMS];
} __attribute__ ((__packed__)) wrc_cal_data_t;

struct spi_flash_device;

struct storage_rwops
{
	int (*read)( struct storage_device*, int offset, void *buf, int count );
	int (*write)( struct storage_device*, int offset, void *buf, int count );
	int (*erase)( struct storage_device*, int offset, int count );
};

struct storage_device
{
	char *name;
	void *priv;
	uint32_t block_size;
	uint32_t size;
	uint32_t cfg_entry;
	int32_t *entry_points;
	struct storage_rwops *rwops;
	int flags;
};

extern struct storage_device wrc_storage_dev;

void storage_spiflash_create(struct storage_device *dev, struct spi_flash_device *flash);

void storage_init( struct i2c_bus *bus, int i2c_addr);

int storage_sfpdb_erase(void);
int storage_match_sfp(struct s_sfpinfo *sfp);
int storage_get_sfp(struct s_sfpinfo *sfp, uint8_t add, uint8_t pos);

int storage_phtrans(uint32_t *val, uint8_t write);

int storage_init_erase(void);
int storage_init_add(const char *args[]);
int storage_init_show(void);
int storage_init_readcmd(uint8_t *buf, uint8_t bufsize, uint8_t next);
int storage_sdbfs_erase( struct storage_device *dev, uint32_t addr, int force_base );
int storage_sdbfs_format( struct storage_device *dev, uint32_t addr, int force_base );
void storage_sdbfs_list(void);

int storage_get_calibration_parameter( int id, uint32_t *valp );
int storage_set_calibration_parameter( int id, uint32_t val );
wrc_cal_data_t* storage_get_calibration_data(void);
int storage_load_calibration(void);
int storage_save_calibration(void);

int storage_read_hdl_cfg(void);


#endif
