/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Theodor Stana <t.stana@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <wrc.h>
#include <flash.h>
#include <types.h>
#include <storage.h>

#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

#include "syscon.h"
#include "dev/spi.h"
#include "dev/spi_flash.h"

static struct spi_bus spi_wrc_flash;
struct spi_flash_device wrc_flash_dev;

/*****************************************************************************/
/*			SDB						     */
/*****************************************************************************/

/* The sdb filesystem itself */
static struct sdbfs wrc_sdb = {
	.name = "wrpc-storage",
	.blocksize = 1, /* Not currently used */
	/* .read and .write according to device type */
};

/*
 * SDB read and write functions
 */
static int sdb_flash_read(struct sdbfs *fs, int offset, void *buf, int count)
{
	return spi_flash_read( &wrc_flash_dev, offset, buf, count );
}

static int sdb_flash_write(struct sdbfs *fs, int offset, void *buf, int count)
{
	return spi_flash_write( &wrc_flash_dev, offset, buf, count );
}


/*
 * A trivial dumper, just to show what's up in there
 */
static void flash_sdb_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;

	while ((d = sdbfs_scan(fs, new)) != NULL) {
		d->sdb_component.product.record_type = '\0';
		pp_printf("file 0x%08x @ %4i, name %19s\n",
			  (int)(d->sdb_component.product.device_id),
			  (int)(d->sdb_component.addr_first),
			  (char *)(d->sdb_component.product.name));
		new = 0;
	}
}

/*
 * Check for SDB presence on flash
 */
int flash_sdb_check(void)
{
	uint32_t magic = 0;
	int i;

	uint32_t entry_point[] = {
			0x000000,	/* flash base */
			0x100,		/* second page in flash */
			0x200,		/* IPMI with MultiRecord */
			0x300,		/* IPMI with larger MultiRecord */
			0x170000,	/* after first FPGA bitstream */
			0x2e0000	/* after MultiBoot bitstream */
			};

	for (i = 0; i < ARRAY_SIZE(entry_point); i++) {
		flash_read(entry_point[i], (uint8_t *)&magic, 4);
		if (magic == SDB_MAGIC)
			break;
	}
	if (i == ARRAY_SIZE(entry_point))
		return -1;

	pp_printf("Found SDB magic at address 0x%06x\n", entry_point[i]);
	wrc_sdb.drvdata = NULL;
	wrc_sdb.entrypoint = entry_point[i];
	wrc_sdb.read = sdb_flash_read;
	wrc_sdb.write = sdb_flash_write;
	flash_sdb_list(&wrc_sdb);
	return 0;
}

void	flash_init(void)
{
	pp_printf("flash-init\n");
	bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, 10 );

	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash );
	pp_printf("flash-exit\n");
	return 0;
}