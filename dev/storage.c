/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012, 2013 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <errno.h>
#include <wrc.h>
#include <dev/w1.h>
#include <storage.h>

#include "types.h"
#include "dev/bb_i2c.h"
#include "dev/onewire.h"
#include "dev/endpoint.h"
#include "dev/syscon.h"
#include "dev/spi_flash.h"
#include "dev/i2c_eeprom.h"
#include <sdb.h>

#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>
#include <dev/fram.h>

/*
 * This source file is a drop-in replacement of the legacy one: it manages
 * both i2c and w1 devices even if the interface is the old i2c-based one
 */
#define SDB_VENDOR	0x46696c6544617461LL /* "FileData" */
#define SDB_DEV_INIT	0x77722d69 /* wr-i (nit) */
#define SDB_DEV_MAC	0x6d61632d /* mac- (address) */
#define SDB_DEV_SFP	0x7366702d /* sfp- (database) */
#define SDB_DEV_CALIB	0x63616c69 /* cali (bration) */

/* constants for scanning I2C EEPROMs */
#define EEPROM_START_ADR 0
#define EEPROM_STOP_ADR  127

#define STORAGE_FLAG_DEVICE_OK (1<<0)

static const uint32_t sdbfs_default_bin[] =
{
	#include "generated/sdbfs-default.h"
};

struct storage_device wrc_storage_dev;
struct sdbfs wrc_sdbfs;

struct storage_device;



struct storage_fram_priv
{
	struct fram_device *dev;
};

struct storage_w1_priv
{
	struct w1_bus *dev;
};

struct storage_i2c_eeprom_priv
{
	struct i2c_eeprom_device *dev;
};

/* Functions for Flash access */
static int sdb_flash_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_read( priv ,offset, buf, count);
}

static int sdb_flash_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_write( priv, offset, buf, count);
}

static int sdb_flash_erase(struct storage_device *dev, int offset, int count)
{
	struct spi_flash_device *priv = (struct spi_flash_device* ) dev->priv;
	return spi_flash_erase( priv, offset, count);
}

const struct storage_rwops spi_flash_rwops = {
	sdb_flash_read,
	sdb_flash_write,
	sdb_flash_erase
};

const int32_t spi_flash_default_entry_points[] = 
{
				0x000000,	/* flash base */
				0x100,		/* second page in flash */
				0x200,		/* IPMI with MultiRecord */
				0x300,		/* IPMI with larger MultiRecord */
				0x170000,	/* after first FPGA bitstream */
				0x2e0000,	/* after MultiBoot bitstream */
				0x600000,	/* after SVEC AFPGA bitstream */
				-1 };

const int32_t i2c_eeprom_default_entry_points[] = 
{
				0x000000,	/* eeprom base */
				0x100,		/* second page in eeprom */
				0x200,		/* IPMI with MultiRecord */
				0x300,		/* IPMI with larger MultiRecord */
				-1 };

/* Functions for FRAM access */
static int sdb_fram_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_read( priv->dev , offset, buf, count);
}

static int sdb_fram_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_write(priv->dev, offset, buf, count);
}

static int sdb_fram_erase(struct storage_device *dev, int offset, int count)
{
	struct storage_fram_priv *priv = (struct storage_fram_priv* ) dev->priv;
	return fram_erase(priv->dev, offset, count);
}

const struct storage_rwops spi_fram_rwops = {
	sdb_fram_read,
	sdb_fram_write,
	sdb_fram_erase
};


/* The methods for W1 access */
static int sdb_w1_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_read_eeprom_bus(priv->dev, offset, buf, count);
}

static int sdb_w1_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_write_eeprom_bus(priv->dev, offset, buf, count);
}

static int sdb_w1_erase(struct storage_device *dev, int offset, int count)
{
	struct storage_w1_priv *priv = (struct storage_w1_priv* ) dev->priv;
	return w1_erase_eeprom_bus(priv->dev, offset, count);
}

const struct storage_rwops spi_w1_rwops = {
	sdb_w1_read,
	sdb_w1_write,
	sdb_w1_erase
};


/* The methods for W1 access */
static int sdb_i2c_eeprom_read(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_i2c_eeprom_priv *priv = (struct storage_i2c_eeprom_priv* ) dev->priv;
	return i2c_eeprom_read(priv->dev, offset, buf, count);
}

static int sdb_i2c_eeprom_write(struct storage_device *dev, int offset, void *buf, int count)
{
	struct storage_i2c_eeprom_priv *priv = (struct storage_i2c_eeprom_priv* ) dev->priv;
	return i2c_eeprom_read(priv->dev, offset, buf, count);
}

static int sdb_i2c_eeprom_erase(struct storage_device *dev, int offset, int count)
{
	struct storage_i2c_eeprom_priv *priv = (struct storage_i2c_eeprom_priv* ) dev->priv;
	return i2c_eeprom_erase(priv->dev, offset, count);
}

/* Functions for I2C EEPROM access */
const struct storage_rwops i2c_eeprom_rwops = {
	sdb_i2c_eeprom_read,
	sdb_i2c_eeprom_write,
	sdb_i2c_eeprom_erase
};

void storage_spiflash_create(struct storage_device *dev, struct spi_flash_device *flash)
{
	static const char* spi_flash_str = "spi-flash";
	dev->name = (char *) spi_flash_str;
	dev->priv = flash;
	dev->rwops = &spi_flash_rwops;
	dev->size = flash->size;
	dev->cfg_entry = flash->cfg_entry;
	dev->block_size = flash->sector_size;
	dev->entry_points = spi_flash_default_entry_points;
	dev->flags = STORAGE_FLAG_DEVICE_OK;
}

void storage_i2c_eeprom_create(struct storage_device *dev, struct i2c_eeprom_device *eeprom)
{
	static const char* i2c_eeprom_str = "eeprom";
	dev->name = (char *) i2c_eeprom_str;
	dev->priv = eeprom;
	dev->rwops = &i2c_eeprom_rwops;
	dev->size = 8192;
	dev->cfg_entry = 0;
	dev->block_size = 1;
	dev->entry_points = i2c_eeprom_default_entry_points;
	dev->flags = STORAGE_FLAG_DEVICE_OK;
}


#if 0
/*
 * A trivial dumper, just to show what's up in there
 */
static void storage_sdb_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;

	while ((d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		pp_printf("file 0x%08x @ 0x%08x, name %s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(d->sdb_component.addr_first),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}
/* The sdb filesystem itself, build-time initialized for i2c */
static struct sdbfs wrc_sdb = {
	.name = "eeprom",
	.blocksize = 1, /* Not currently used */
	.drvdata = &i2c_params,
	.read = sdb_i2c_read,
	.write = sdb_i2c_write,
};

uint8_t has_eeprom = 0; /* modified at init time */

/*
 * Init: sets "int has_eeprom" above
 *
 * This is called by wrc_main, after initializing both w1 and i2c
 */
void storage_init( struct i2c_bus *bus, int chosen_i2c_addr)
{
	uint32_t magic = 0;
	static unsigned entry_points_eeprom[] = {0, 64, 128, 256, 512, 1024};
	static unsigned entry_points_flash[] = {
				0x000000,	/* flash base */
				0x100,		/* second page in flash */
				0x200,		/* IPMI with MultiRecord */
				0x300,		/* IPMI with larger MultiRecord */
				0x170000,	/* after first FPGA bitstream */
				0x2e0000,	/* after MultiBoot bitstream */
				0x600000};	/* after SVEC AFPGA bitstream */
	uint32_t entry_points_fram[] = {
				0x000000,	/* fram base */
				0x6000,
				0x7000
        };
	int i, ret;

	/*
	 * 1. Check if there is SDBFS in the Flash.
	 */
	for (i = 0; i < ARRAY_SIZE(entry_points_flash); i++) {
		spi_flash_read(&wrc_flash_dev, entry_points_flash[i], (void *)&magic, sizeof(magic));
		if (magic == SDB_MAGIC)
			break;
	}
	if (magic == SDB_MAGIC) {
		pp_printf("sdbfs: found at %i in Flash\n",
				entry_points_flash[i]);
		wrc_sdb.drvdata = NULL;
		wrc_sdb.blocksize = storage_cfg.blocksize;
		wrc_sdb.entrypoint = entry_points_flash[i];
		wrc_sdb.read = sdb_flash_read;
		wrc_sdb.write = sdb_flash_write;
		wrc_sdb.erase = sdb_flash_erase;
		goto found_exit;
	}


	/*
	 * 2. Check if there is SDBFS in the FRAM.
	 */
	#if 0
	for (i = 0; i < ARRAY_SIZE(entry_points_fram); i++) {
		fram_read(&wrc_fram_dev, entry_points_fram[i], (void *)&magic, sizeof(magic));
		if (magic == SDB_MAGIC)
			break;
	}
	if (magic == SDB_MAGIC) {
		pp_printf("sdbfs: found at %i in Fram\n",
				entry_points_fram[i]);
		wrc_sdb.drvdata = NULL;
		wrc_sdb.blocksize = 0;
		wrc_sdb.entrypoint = entry_points_fram[i];
		wrc_sdb.read = sdb_fram_read;
		wrc_sdb.write = sdb_fram_write;
		wrc_sdb.erase = sdb_fram_erase;
		goto found_exit;
	}
	#endif
	/*
	 * 3. Look for w1 first: if there is no eeprom it fails fast
	 */
	for (i = 0; i < ARRAY_SIZE(entry_points_eeprom); i++) {
		ret = w1_read_eeprom_bus(&wrpc_w1_bus, entry_points_eeprom[i],
					 (void *)&magic, sizeof(magic));
		if (ret != sizeof(magic))
			break;
		if (magic == SDB_MAGIC)
			break;
	}
	if (magic == SDB_MAGIC) {
		pp_printf("sdbfs: found at %i in W1\n", entry_points_eeprom[i]);
		/* override default i2c settings with w1 ones */
		wrc_sdb.drvdata = &wrpc_w1_bus;
		wrc_sdb.blocksize = 1;
		wrc_sdb.entrypoint = entry_points_eeprom[i];
		wrc_sdb.read = sdb_w1_read;
		wrc_sdb.write = sdb_w1_write;
		wrc_sdb.erase = sdb_w1_erase;
		goto found_exit;
	}

	/*
	 * 3. If w1 failed, look for i2c: start from low offsets.
	 */
	i2c_params.bus = bus;
	i2c_params.addr = EEPROM_START_ADR;
	while (i2c_params.addr <= EEPROM_STOP_ADR) {
		/* First, we check if I2C EEPROM is there */
		if (!bb_i2c_devprobe(bus, i2c_params.addr)) {
			i2c_params.addr++;
			continue;
		}
		/* While looking for the magic number, use sdb-based read function */
		for (i = 0; i < ARRAY_SIZE(entry_points_eeprom); i++) {
			sdb_i2c_read(&wrc_sdbfs, entry_points_eeprom[i], (void *)&magic,
				    sizeof(magic));
			if (magic == SDB_MAGIC)
				break;
		}
		if (magic == SDB_MAGIC) {
			pp_printf("sdbfs: found at %i in I2C(0x%2X)\n",
				entry_points_eeprom[i], i2c_params.addr);
			wrc_sdb.drvdata = &i2c_params;
			wrc_sdb.blocksize = 1;
			wrc_sdb.entrypoint = entry_points_eeprom[i];
			wrc_sdb.read = sdb_i2c_read;
			wrc_sdb.write = sdb_i2c_write;
			wrc_sdb.erase = sdb_i2c_erase;
			goto found_exit;
		}
		i2c_params.addr++;
	}

	if (i2c_params.addr == EEPROM_STOP_ADR) {
		pp_printf("No SDB filesystem in i2c eeprom\n");
		return;
	}

found_exit:
	/* found: register the filesystem */
	has_eeprom = 1;
	sdbfs_dev_create(&wrc_sdb);
	storage_sdb_list(&wrc_sdb);
	return;
}

/*
 * Reading/writing the MAC address used to be part of dev/onewire.c,
 * but is not onewire-specific.  What is w1-specific is the default
 * setting if no sdbfs is there, but CONFIG_SDB_STORAGE depends on
 * CONFIG_W1 anyways.
 */
int get_persistent_mac(uint8_t portnum, uint8_t *mac)
{
	int ret = 0;
	int i;
	struct w1_dev *d;

	if (IS_HOST_PROCESS) {
		/* we don't have sdb working, so get the real eth address */
		ep_get_mac_addr(mac);
		return 0;
	}

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_MAC) < 0)
		ret =-1;
	else
        {
		ret = sdbfs_fread(&wrc_sdbfs, 0, mac, 6);
		sdbfs_close(&wrc_sdb);
        }

	if (ret < 0)
		pp_printf("%s: SDB error\n", __func__);
	if (mac[0] == 0xff ||
	    (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0) {
		pp_printf("%s: SDB file is empty\n", __func__);
		ret = -1;
	}
	if (ret < 0) {
		pp_printf("%s: Using W1 serial number\n", __func__);
		w1_scan_bus(&wrpc_w1_bus);
		for (i = 0; i < W1_MAX_DEVICES; i++) {
			d = wrpc_w1_bus.devs + i;
			if (d->rom) {
				mac[0] = 0x22;
				mac[1] = 0x33;
				mac[2] = 0xff & (d->rom >> 32);
				mac[3] = 0xff & (d->rom >> 24);
				mac[4] = 0xff & (d->rom >> 16);
				mac[5] = 0xff & (d->rom >> 8);
				ret = 0;
				break;
			}
                }
	}
	if (ret < 0) {
		pp_printf("%s: failure\n", __func__);
		return -1;
	}
	return 0;
}

int set_persistent_mac(uint8_t portnum, uint8_t *mac)
{
	int ret;

	ret = sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_MAC);
	if (ret >= 0) {
		sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdb.f_len);
		ret = sdbfs_fwrite(&wrc_sdbfs, 0, mac, 6);
	}
	sdbfs_close(&wrc_sdb);

	if (ret < 0) {
		pp_printf("%s: SDB error, can't save\n", __func__);
		return -1;
	}
	return 0;
}

#endif


/*
 * The SFP section is placed somewhere inside EEPROM (W1 or I2C), using sdbfs.
 *
 * Initially we have a count of SFP records
 *
 * For each sfp we have
 *
 * - part number (16 bytes)
 * - alpha (4 bytes)
 * - deltaTx (4 bytes)
 * - delta Rx (4 bytes)
 * - checksum (1 byte)  (low order 8 bits of the sum of all bytes)
 *
 * the total is 29 bytes for each sfp (ugly, but we are byte-oriented anyways
 */


/* Erase SFB database in the memory */
int storage_sfpdb_erase(void)
{
	int ret;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_SFP) < 0)
		return -1;
	ret = sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdbfs.f_len);
	if (ret == wrc_sdbfs.f_len)
		ret = 1;
	sdbfs_close(&wrc_sdbfs);
	return ret == 1 ? 0 : -1;
}

/* Dummy check if sfp information is correct by verifying it doesn't have
 * 0xff bytes */
static int sfp_valid(struct s_sfpinfo *sfp)
{
	int i;

	for (i = 0; i < SFP_PN_LEN; ++i) {
		if (sfp->pn[i] == 0xff)
			return 0;
	}
	return 1;
}

static int sfp_entry(struct s_sfpinfo *sfp, uint8_t oper, uint8_t pos)
{
	static uint8_t sfpcount = 0;
	struct s_sfpinfo tempsfp;
	int ret = -1;
	uint8_t i, chksum = 0;
	uint8_t *ptr;
	int sdb_offset;

	if (pos >= SFPS_MAX)
		return EE_RET_POSERR;	/* position outside the range */

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_SFP) < 0)
		return -1;

	/* Read how many SFPs are in the database, but only in the first
	 * call */
	if (!pos) {
		sfpcount = 0;
		sdb_offset = sizeof(sfpcount);
		while (sdbfs_fread(&wrc_sdbfs, sdb_offset, &tempsfp,
					sizeof(tempsfp)) == sizeof(tempsfp)) {
			if (!sfp_valid(&tempsfp))
				break;
			sfpcount++;
			sdb_offset = sizeof(sfpcount) + sfpcount * sizeof(tempsfp);
		}
	}

	if ((oper == SFP_ADD) && (sfpcount == SFPS_MAX)) {
		/* no more space to add new SFPs */
		ret = EE_RET_DBFULL;
		goto out;
	}

	if (!pos && (oper == SFP_GET) && sfpcount == 0) {
		/* no SFPs in the database */
		ret = 0;
		goto out;
	}

	if (oper == SFP_GET) {
		sdb_offset = sizeof(sfpcount) + pos * sizeof(*sfp);
		if (sdbfs_fread(&wrc_sdbfs, sdb_offset, sfp, sizeof(*sfp))
				!= sizeof(*sfp))
			goto out;

		ptr = (uint8_t *)sfp;
		/* read sizeof() - 1 because we don't include checksum */
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)
			chksum = chksum + *(ptr++);
		if (chksum != sfp->chksum) {
			pp_printf("sfp: corrupted checksum\n");
			goto out;
		}
	}
	if (oper == SFP_ADD) {
		/* count checksum */
		ptr = (uint8_t *)sfp;
		/* use sizeof() - 1 because we don't include checksum */
		for (i = 0; i < sizeof(struct s_sfpinfo) - 1; ++i)
			chksum = chksum + *(ptr++);
		sfp->chksum = chksum;
		/* add SFP at the end of DB */
		sdb_offset = sizeof(sfpcount) + sfpcount * sizeof(*sfp);
		if (sdbfs_fwrite(&wrc_sdbfs, sdb_offset, sfp, sizeof(*sfp))
				!= sizeof(*sfp)) {
			goto out;
		}
		sfpcount++;
	}
	ret = sfpcount;
out:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

static int storage_update_sfp(struct s_sfpinfo *sfp)
{
	int sfpcount = 1;
	int temp;
	int8_t i;
	struct s_sfpinfo sfp_db[SFPS_MAX];
	struct s_sfpinfo *dbsfp;

	/* copy entries from flash to the memory, update entry if matched */
	for (i = 0; i < sfpcount; ++i) {
		dbsfp = &sfp_db[i];
		sfpcount = sfp_entry(dbsfp, SFP_GET, i);
		if (sfpcount <= 0)
			return sfpcount;
		if (!strncmp(dbsfp->pn, sfp->pn, 16)) {
			/* update matched entry */
			dbsfp->dTx = sfp->dTx;
			dbsfp->dRx = sfp->dRx;
			dbsfp->alpha = sfp->alpha;
		}
	}

	/* erase entire database */
	if (storage_sfpdb_erase() == EE_RET_I2CERR) {
			pp_printf("Could not erase DB\n");
			return -1;
		}

	/* add all SFPs */
	for (i = 0; i < sfpcount; ++i) {
		dbsfp = &sfp_db[i];
		temp = sfp_entry(dbsfp, SFP_ADD, 0);
		if (temp < 0) {
			/* if error, return it */
			return temp;
		}
	}
	return i;
}

int storage_get_sfp(struct s_sfpinfo *sfp, uint8_t oper, uint8_t pos)
{
	struct s_sfpinfo tmp_sfp;

	if (oper == SFP_GET) {
		/* Get SFP entry */
		return sfp_entry(sfp, SFP_GET, pos);
	}

	/* storage_match_sfp replaces content of parameter, so do the copy
	 * first */
	tmp_sfp = *sfp;
	if (!storage_match_sfp(&tmp_sfp)) { /* add a new sfp entry */
		pp_printf("Adding new SFP entry\n");
		return sfp_entry(sfp, SFP_ADD, 0);
	}

	pp_printf("Update existing SFP entry\n");
	return storage_update_sfp(sfp);
}

int storage_match_sfp(struct s_sfpinfo *sfp)
{
	uint8_t sfpcount = 1;
	int8_t i;
	struct s_sfpinfo dbsfp;

	for (i = 0; i < sfpcount; ++i) {
		sfpcount = sfp_entry(&dbsfp, SFP_GET, i);
		if (sfpcount <= 0)
			return sfpcount;
		if (!strncmp(dbsfp.pn, sfp->pn, 16)) {
			sfp->dTx = dbsfp.dTx;
			sfp->dRx = dbsfp.dRx;
			sfp->alpha = dbsfp.alpha;
			return 1;
		}
	}
	return 0;

}





/*
 * Calibration File Functions
 */

static wrc_cal_data_t cal_data;

static int calc_checksum( wrc_cal_data_t* cal )
{
	int i;
	uint32_t cksum;
	cksum += cal->magic;
	cksum += cal->param_count;
	for(i = 0; i < cal->param_count; i++)
	{
		cksum += cal->params[i].id;
		cksum += cal->params[i].value;
	}

	return cksum;
}

int storage_get_calibration_parameter( int id, uint32_t *valp )
{
	int i;

	for(i = 0; i < cal_data.param_count; i++)
	{
		if ( id == cal_data.params[i].id )
		{
			*valp = cal_data.params[i].value;
			return 0;
		}
	}

	return -1;
}

int storage_set_calibration_parameter( int id, uint32_t val )
{
	int i;

	for(i = 0; i < cal_data.param_count; i++)
	{
		if ( id == cal_data.params[i].id )
		{
			cal_data.params[i].value = val;
			return storage_save_calibration();;
		}
	}

	if( cal_data.param_count >= CAL_MAX_PARAMS )
		return -1;

	cal_data.params[cal_data.param_count].id = id;
	cal_data.params[cal_data.param_count].value = val;
	cal_data.param_count ++;


	return storage_save_calibration();
}

wrc_cal_data_t* storage_get_calibration_data(void)
{
	return &cal_data;
}


int storage_load_calibration(void)
{
	int ret = 0;
	int i;

	cal_data.param_count = 0;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_CALIB) < 0)
	{
		storage_dbg("%s: can't open cal file\n", __FUNCTION__);
		return -1;
	}	

	if (sdbfs_fread(&wrc_sdbfs, 0, &cal_data, sizeof(cal_data))
		    != sizeof(cal_data))
	{
		ret = -1;
		cal_data.param_count = 0;
		goto out_close;
	}

	if( cal_data.magic != CAL_FILE_MAGIC )
	{
		storage_dbg("%s: invalid magic\n", __FUNCTION__);
		cal_data.param_count = 0;
		ret = -1;
		goto out_close;
	}

	uint32_t cksum = calc_checksum( &cal_data );

	if( cal_data.checksum != cksum )
	{
		storage_dbg("%s: invalid checksum %x vs %x\n", __FUNCTION__, cal_data.checksum, cksum );
		cal_data.param_count = 0;
		ret = -1;
		goto out_close;
	}


	storage_dbg("Loaded %d calibration params, checksum = 0x%x\n", cal_data.param_count, cal_data.checksum );

	for(i = 0; i < cal_data.param_count; i++)
	{
		storage_dbg( " - param %c%c%c%c = %d\n", 
			((cal_data.params[i].id) >> 24) & 0xff,
			((cal_data.params[i].id) >> 16) & 0xff,
			((cal_data.params[i].id) >> 8) & 0xff,
			((cal_data.params[i].id) >> 0) & 0xff,
			  cal_data.params[i].value );
	}

out_close:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

int storage_save_calibration(void)
{
	int ret = 0;
	int i;

	cal_data.magic = CAL_FILE_MAGIC;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_CALIB) < 0)
	{
		storage_dbg("%s: can't open calibration file\n", __FUNCTION__);
		return -1;
	}

	cal_data.checksum = calc_checksum( &cal_data );

	sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdbfs.f_len);

	if (sdbfs_fwrite(&wrc_sdbfs, 0, &cal_data, sizeof(cal_data))
	    != sizeof(cal_data))
			goto out_close;

	storage_dbg("Saved %d bytes of calibration data:\n", sizeof(cal_data ));

	for(i = 0; i < cal_data.param_count; i++)
	{
		storage_dbg( " - param %c%c%c%c = %d\n", 
			((cal_data.params[i].id) >> 24) & 0xff,
			((cal_data.params[i].id) >> 16) & 0xff,
			((cal_data.params[i].id) >> 8) & 0xff,
			((cal_data.params[i].id) >> 0) & 0xff,
			  cal_data.params[i].value );
	}


out_close:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

// FIXME: migrate to new API
int storage_phtrans(uint32_t *valp, uint8_t write)
{
	if( !write )
		return storage_get_calibration_parameter( CAL_PARAM_T24P, valp );
	else
		return storage_set_calibration_parameter( CAL_PARAM_T24P, *valp );
}


/* MAC Address Storage */

int get_persistent_mac(uint8_t portnum, uint8_t *mac)
{
	int ret = 0;
	int i;
	struct w1_dev *d;

	// fixme: we should mock entire storage in a host process, not put
	// such compile-time ifs() in target code
	if (IS_HOST_PROCESS) {
		/* we don't have sdb working, so get the real eth address */
		ep_get_mac_addr(mac);
		return 0;
	}

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_MAC) < 0)
		ret =-1;
	else
        {
		ret = sdbfs_fread(&wrc_sdbfs, 0, mac, 6);
		sdbfs_close(&wrc_sdbfs);
        }

	if (ret < 0)
		storage_dbg("%s: SDB error\n", __func__);
	if (mac[0] == 0xff ||
	    (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0) {
		storage_dbg("%s: SDB file is empty\n", __func__);
		ret = -1;
	}
	
	// fixme: use SDB and only SDB here. Fallback onewire MAC should go to the 'generic' board target.
	#if 0
	if (ret < 0) {
		pp_printf("%s: Using W1 serial number\n", __func__);
		w1_scan_bus(&wrpc_w1_bus);
		for (i = 0; i < W1_MAX_DEVICES; i++) {
			d = wrpc_w1_bus.devs + i;
			if (d->rom) {
				mac[0] = 0x22;
				mac[1] = 0x33;
				mac[2] = 0xff & (d->rom >> 32);
				mac[3] = 0xff & (d->rom >> 24);
				mac[4] = 0xff & (d->rom >> 16);
				mac[5] = 0xff & (d->rom >> 8);
				ret = 0;
				break;
			}
                }
	}
	#endif
	if (ret < 0) {
		storage_dbg("%s: failure\n", __func__);
		return -1;
	}
	return 0;
}

int set_persistent_mac(uint8_t portnum, uint8_t *mac)
{
	int ret;

	ret = sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_MAC);
	if (ret >= 0) {
		sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdbfs.f_len);
		ret = sdbfs_fwrite(&wrc_sdbfs, 0, mac, 6);
	}
	sdbfs_close(&wrc_sdbfs);

	if (ret < 0) {
		storage_dbg("%s: SDB error, can't save\n", __func__);
		return -1;
	}
	return 0;
}

/*
 * The init script area consist of 2-byte size field and a set of
 * shell commands separated with '\n' character.
 *
 * -------------------
 * | bytes used (2B) |
 * ------------------------------------------------
 * | shell commands separated with '\n'.....      |
 * |                                              |
 * |                                              |
 * ------------------------------------------------
 */

int storage_init_erase(void)
{
	int ret;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;
	ret = sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdbfs.f_len);
	if (ret == wrc_sdbfs.f_len)
		ret = 1;
	sdbfs_close(&wrc_sdbfs);
	return ret == 1 ? 0 : -1;
}

/*
 * Appends a new shell command at the end of boot script
 */
int storage_init_add(const char *args[])
{
	int len, i;
	uint8_t separator = ' ';
	uint16_t used, readback;
	int ret = -1;
	uint8_t byte;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;

	/* check how many bytes we already have there */
	used = 0;
	while (sdbfs_fread(&wrc_sdbfs, sizeof(used)+used, &byte, 1) == 1) {
		if (byte == 0xff)
			break;
		used++;
	}

	if (used > 256 /* 0xffff or wrong */)
		used = 0;

	i = 1; /* args[0] is "add" */
	while (args[i] != NULL) {
		len = strlen(args[i]);
		if (sdbfs_fwrite(&wrc_sdbfs, sizeof(used) + used,
				 (void *)args[i], len) != len)
			goto out;
		used += len;
		if (args[i+1] != NULL)	/* next one is another word of the same command */
			separator = ' ';
		else			/* no more words, end command with '\n' */
			separator = '\n';
		if (sdbfs_fwrite(&wrc_sdbfs, sizeof(used) + used,
				&separator, sizeof(separator))
		    != sizeof(separator))
			goto out;
		++used;
		++i;
	}
	/* and finally update the size of the script */
	if (sdbfs_fwrite(&wrc_sdbfs, 0, &used, sizeof(used)) != sizeof(used))
		goto out;

	if (sdbfs_fread(&wrc_sdbfs, 0, &readback, sizeof(readback))
	    != sizeof(readback))
		goto out;

	ret = 0;
out:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

int storage_init_show(void)
{
	int ret = -1;
	uint16_t used;
	uint8_t byte;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;

	pp_printf("-- user-defined script --\n");
	used = 0;
	do {
		if (sdbfs_fread(&wrc_sdbfs, sizeof(used) + used, &byte, 1) != 1)
			goto out;
		if (byte != 0xff) {
			pp_printf("%c", byte);
			used++;
		}
	} while (byte != 0xff);

	if (used == 0)
		pp_printf("(empty)\n");
	ret = 0;
out:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

int storage_init_readcmd(uint8_t *buf, uint8_t bufsize, uint8_t next)
{
	int i = 0, ret = -1;
	uint16_t used;
	static uint16_t ptr;

	if (sdbfs_open_id(&wrc_sdbfs, SDB_VENDOR, SDB_DEV_INIT) < 0)
		return -1;

	if (next == 0)
		ptr = sizeof(used);
	do {
		if (i > bufsize)
			goto out;
		if (sdbfs_fread(&wrc_sdbfs, (ptr++),
				&buf[i], sizeof(char)) != sizeof(char))
			goto out;
		if (buf[i] == 0xff)
			break;
	} while (buf[i++] != '\n');
	ret = i;
out:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}



/* glue callback code to pass storage_rwops to sdbfs */
static int sdbfs_read_callback(struct sdbfs *fs, int offset, void *buf, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->read( dev, offset, buf, count );
}

static int sdbfs_write_callback(struct sdbfs *fs, int offset, void *buf, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->write( dev, offset, buf, count );
}

static int sdbfs_erase_callback(struct sdbfs *fs, int offset, int count)
{
	struct storage_device *dev = (struct storage_device*) fs->drvdata;
	return dev->rwops->erase( dev, offset, count );
}


int storage_mount( struct storage_device *dev )
{
	uint32_t magic = 0;
	int i, ret;

	/* Check if there is SDBFS in the memory */

	storage_dbg("mounting '%s' [%d bytes]\n", dev->name, dev->size );

	for (i = 0; dev->entry_points[i] >= 0; i++)
	{
		if( dev->entry_points[i] < dev->size )
		{
			storage_dbg("try entry point 0x%08x\n", dev->entry_points[i] );
			dev->rwops->read( dev, dev->entry_points[i], (void *)&magic, sizeof(magic) );
			if (magic == SDB_MAGIC)
				break;
		}
	}

	/* found? mount it! */
	if (magic == SDB_MAGIC) {
		storage_dbg("found SDBFS at 0x%x in device '%s'\n",
				dev->entry_points[i], dev->name );
		wrc_sdbfs.drvdata = dev;
		wrc_sdbfs.blocksize = dev->block_size;
		wrc_sdbfs.entrypoint = dev->entry_points[i];
		wrc_sdbfs.read = sdbfs_read_callback;
		wrc_sdbfs.write = sdbfs_write_callback;
		wrc_sdbfs.erase = sdbfs_erase_callback;
		return 0;
	}

	storage_dbg("SDBFS not found.\n");

	return -ENODEV;
}


static inline unsigned long SDB_ALIGN(unsigned long x, int blocksize)
{
	return (x + (blocksize - 1)) & ~(blocksize - 1);
}

int storage_sdbfs_erase( struct storage_device *dev, uint32_t addr, int force_base )
{
	int total_size = SDBFS_REC * wrc_sdbfs.blocksize;
	int count = 0;
	uint32_t base_addr;

	if (force_base)
		base_addr = addr;
	else
		base_addr = dev->cfg_entry;

	wrc_sdbfs.drvdata = dev;
	wrc_sdbfs.blocksize = dev->block_size;

	while( count < total_size )
	{
		sdbfs_erase_callback( &wrc_sdbfs, base_addr + count, wrc_sdbfs.blocksize );
		count +=  wrc_sdbfs.blocksize;
	}
}

int storage_sdbfs_format( struct storage_device *dev, uint32_t addr, int force_base )
{
	struct sdb_device *sdbfs =
		 (struct sdb_device *) sdbfs_default_bin;
	struct sdb_interconnect *sdbfs_dir = (struct sdb_interconnect *)
		sdbfs_default_bin;
	struct sdb_device sdbfs_buf[SDBFS_REC];

	int i;
	char buf[19] = {0};
	int cur_adr, size;
	uint32_t val;
	uint32_t base_addr;

	if (force_base)
		base_addr = addr;
	else
		base_addr = dev->cfg_entry;

	wrc_sdbfs.drvdata = dev;
	wrc_sdbfs.blocksize = dev->block_size;

	/* first file starts after the SDBFS description */
	cur_adr = base_addr + SDB_ALIGN(SDBFS_REC*sizeof(struct sdb_device),
			wrc_sdbfs.blocksize );

	/* scan through files */
	for (i = 1; i < SDBFS_REC; ++i) {
		/* relocate each file depending on base address and block size*/
		size = sdbfs[i].sdb_component.addr_last -
			sdbfs[i].sdb_component.addr_first;
		sdbfs[i].sdb_component.addr_first = cur_adr;
		sdbfs[i].sdb_component.addr_last  = cur_adr + size;
		cur_adr = SDB_ALIGN(cur_adr + (size + 1), wrc_sdbfs.blocksize);
	}
	/* update the directory */
	sdbfs_dir->sdb_component.addr_first = base_addr;
	sdbfs_dir->sdb_component.addr_last  =
		sdbfs[SDBFS_REC-1].sdb_component.addr_last;

	for (i = 0; i < SDBFS_REC; ++i) 
	{
		strncpy(buf, (char *)sdbfs[i].sdb_component.product.name, 18);
		pp_printf("filename: %s; first: %x; last: %x\n", buf,
				(int)sdbfs[i].sdb_component.addr_first,
				(int)sdbfs[i].sdb_component.addr_last);
	}

	
	pp_printf("Formatting SDBFS in %s (base 0x%08x, size 0x%08x)...\n", dev->name, base_addr, SDBFS_REC * wrc_sdbfs.blocksize );

	storage_sdbfs_erase(dev, addr, force_base);

	size = sizeof(struct sdb_device);

	for (i = 0; i < SDBFS_REC; ++i) {
		sdbfs_write_callback(&wrc_sdbfs, base_addr + i*size, &sdbfs[i],
				size);
	}

	pp_printf("Verification...\n");
		sdbfs_read_callback( &wrc_sdbfs, base_addr, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device));
		if(memcmp(sdbfs, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device)))
			pp_printf("Error.\n");
		else
			pp_printf("OK.\n");

	return storage_mount( dev );
}


/*
 * A trivial dumper, just to show what's up in there
 */
void storage_sdbfs_list()
{
	struct sdbfs *fs = &wrc_sdbfs;
	struct sdb_device *d;
	int new = 1;

	while ((d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		pp_printf("file 0x%08x @ 0x%08x, name %19s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(d->sdb_component.addr_first),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}
