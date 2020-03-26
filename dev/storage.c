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


struct storage_device wrc_storage_dev;
struct sdbfs wrc_sdbfs;

// fixme: remove
uint8_t has_eeprom = 0;

struct storage_device;

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
	int32_t *entry_points;
	struct storage_rwops *rwops;
};


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


void storage_spiflash_create(struct storage_device *dev, struct spi_flash_device *flash)
{
	static const char* spi_flash_str = "spi-flash";
	dev->name = (char *) spi_flash_str;
	dev->priv = flash;
	dev->rwops = &spi_flash_rwops;
	dev->size = flash->size;
	dev->block_size = flash->sector_size;
	dev->entry_points = spi_flash_default_entry_points;
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
	ret = sdbfs_ferase(&wrc_sdbfs, 0, wrc_sdb.f_len);
	if (ret == wrc_sdb.f_len)
		ret = 1;
	sdbfs_close(&wrc_sdb);
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
	sdbfs_close(&wrc_sdb);
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
#endif

int storage_match_sfp(struct s_sfpinfo *sfp)
{
#if 0
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
#endif
	return 0;

}





#if 0

extern uint32_t _binary_tools_sdbfs_default_bin_start[];
extern uint32_t _binary_tools_sdbfs_default_bin_end[];

static inline unsigned long SDB_ALIGN(unsigned long x, int blocksize)
{
	return (x + (blocksize - 1)) & ~(blocksize - 1);
}

int storage_sdbfs_erase(int mem_type, uint32_t base_adr, uint32_t blocksize,
		uint8_t i2c_adr)
{
	if (!HAS_GENSDBFS || (mem_type == MEM_FLASH && blocksize == 0))
		return -EINVAL;

	if (mem_type == MEM_FLASH) {
		pp_printf("Erasing Flash(0x%x)...\n", base_adr);
		sdb_flash_erase(NULL, base_adr, SDBFS_REC * blocksize);
	} else if (mem_type == MEM_EEPROM) {
		pp_printf("Erasing EEPROM %d (0x%x)...\n", i2c_adr, base_adr);
		i2c_params.bus = &dev_i2c_fmc;
		i2c_params.addr  = i2c_adr;
		wrc_sdb.drvdata = &i2c_params;
		sdb_i2c_erase(&wrc_sdbfs, base_adr, SDBFS_REC *
				sizeof(struct sdb_device));
	} else if (mem_type == MEM_1W_EEPROM) {
		pp_printf("Erasing 1-W EEPROM (0x%x)...\n", base_adr);
		wrc_sdb.drvdata = &wrpc_w1_bus;
		sdb_w1_erase(&wrc_sdbfs, base_adr, SDBFS_REC *
				sizeof(struct sdb_device));
	} else 	if (mem_type == MEM_FRAM) {
		pp_printf("Erasing FRAM (0x%x)...\n", base_adr);
		sdb_fram_erase(NULL, base_adr, SDBFS_REC * blocksize);
        }
	return 0;
}

int storage_gensdbfs(int mem_type, uint32_t base_adr, uint32_t blocksize,
		uint8_t i2c_adr)
{
	struct sdb_device *sdbfs =
		(struct sdb_device *) _binary_tools_sdbfs_default_bin_start;
	struct sdb_interconnect *sdbfs_dir = (struct sdb_interconnect *)
		_binary_tools_sdbfs_default_bin_start;
	/* struct sdb_device sdbfs_buf[SDBFS_REC]; */
	int i;
	char buf[19] = {0};
	int cur_adr, size;
	uint32_t val;

	if (!HAS_GENSDBFS || (mem_type == MEM_FLASH && base_adr == 0))
		return -EINVAL;

	if (mem_type == MEM_FLASH && blocksize == 0)
		return -EINVAL;

	/* first file starts after the SDBFS description */
	cur_adr = base_adr + SDB_ALIGN(SDBFS_REC*sizeof(struct sdb_device),
			blocksize);
	/* scan through files */
	for (i = 1; i < SDBFS_REC; ++i) {
		/* relocate each file depending on base address and block size*/
		size = sdbfs[i].sdb_component.addr_last -
			sdbfs[i].sdb_component.addr_first;
		sdbfs[i].sdb_component.addr_first = cur_adr;
		sdbfs[i].sdb_component.addr_last  = cur_adr + size;
		cur_adr = SDB_ALIGN(cur_adr + (size + 1), blocksize);
	}
	/* update the directory */
	sdbfs_dir->sdb_component.addr_first = base_adr;
	sdbfs_dir->sdb_component.addr_last  =
		sdbfs[SDBFS_REC-1].sdb_component.addr_last;

	for (i = 0; i < SDBFS_REC; ++i) {
		strncpy(buf, (char *)sdbfs[i].sdb_component.product.name, 18);
		pp_printf("filename: %s; first: %x; last: %x\n", buf,
				(int)sdbfs[i].sdb_component.addr_first,
				(int)sdbfs[i].sdb_component.addr_last);
	}

	size = sizeof(struct sdb_device);
	if (mem_type == MEM_FLASH) {
		pp_printf("Formatting SDBFS in Flash(0x%x)...\n", base_adr);
		/* each file is in a separate block, therefore erase SDBFS_REC
		 * number of blocks */
		sdb_flash_erase(NULL, base_adr, SDBFS_REC * blocksize);
		for (i = 0; i < SDBFS_REC; ++i) {
			sdb_flash_write(NULL, base_adr + i*size, &sdbfs[i],
					size);
		}
		/*
		pp_printf("Verification...");
		sdb_flash_read(NULL, base_adr, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device));
		if(memcmp(sdbfs, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device)))
			pp_printf("Error.\n");
		else
			pp_printf("OK.\n");
		*/
	} else if (mem_type == MEM_EEPROM) {
		/* First, check if EEPROM is really there */
		if (!bb_i2c_devprobe(&dev_i2c_fmc, i2c_adr)) {
			pp_printf("I2C EEPROM not found\n");
			return -EINVAL;
		}
		i2c_params.bus = &dev_i2c_fmc;
		i2c_params.addr  = i2c_adr;
		pp_printf("Formatting SDBFS in I2C EEPROM %d (0x%x)...\n",
				i2c_params.addr, base_adr);
		wrc_sdb.drvdata = &i2c_params;
		sdb_i2c_erase(&wrc_sdbfs, base_adr, SDBFS_REC * size);
		for (i = 0; i < SDBFS_REC; ++i) {
			sdb_i2c_write(&wrc_sdbfs, base_adr + i*size, &sdbfs[i],
					size);
		}
		/*
		pp_printf("Verification...");
		sdb_i2c_read(&wrc_sdbfs, base_adr, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device));
		if(memcmp(sdbfs, sdbfs_buf, SDBFS_REC *
				sizeof(struct sdb_device)))
			pp_printf("Error.\n");
		else
			pp_printf("OK.\n");
		*/
	} else if (mem_type == MEM_1W_EEPROM) {
		wrc_sdb.drvdata = &wrpc_w1_bus;
		if (sdb_w1_read(&wrc_sdbfs, 0, &val, sizeof(val)) !=
				sizeof(val)) {
			pp_printf("1-Wire EEPROM not found\n");
			return -EINVAL;
		}
		pp_printf("Formatting SDBFS in 1-W EEPROM (0x%x)...\n",
				base_adr);
		sdb_w1_erase(&wrc_sdbfs, base_adr, SDBFS_REC * size);
		for (i = 0; i < SDBFS_REC; ++i) {
			sdb_w1_write(&wrc_sdbfs, base_adr + i*size, &sdbfs[i],
					size);
		}
	} else if (mem_type == MEM_FRAM) {
		pp_printf("Formatting SDBFS in FRAM(0x%x)...\n", base_adr);
		sdb_fram_erase(NULL, base_adr, SDBFS_REC * size);
		for (i = 0; i < SDBFS_REC; ++i) {
			sdb_fram_write(NULL, base_adr + i*size, &sdbfs[i],
					size);
		}
        }
	/* re-initialize storage after writing sdbfs image */
	storage_init( &dev_i2c_fmc, FMC_EEPROM_ADR);

	return mem_type;
}

#endif


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
			return 0;
		}
	}

	if( cal_data.param_count >= CAL_MAX_PARAMS )
		return -1;

	cal_data.params[cal_data.param_count].id = id;
	cal_data.params[cal_data.param_count].value = val;
	cal_data.param_count ++;
}

wrc_cal_data_t* storage_get_calibration_data(void)
{
	return &cal_data;
}


int storage_load_calibration(void)
{
	int ret = 0;
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

	if( cal_data.checksum != calc_checksum( &cal_data ))
	{
		storage_dbg("%s: invalid checksum\n", __FUNCTION__);
		cal_data.param_count = 0;
		ret = -1;
		goto out_close;
	}

out_close:
	sdbfs_close(&wrc_sdbfs);
	return ret;
}

int storage_save_calibration(void)
{
	int ret = 0;

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
		return storage_set_calibration_parameter( CAL_PARAM_T24P, valp );
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

