/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Theodor Stana <t.stana@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <types.h>

#include "dev/spi.h"
#include "dev/spi_flash.h"

static uint8_t spi_flash_rsr(struct spi_flash_device *dev);

/*
 * Init function (just set the SPI pins for idle)
 */
void spi_flash_create(struct spi_flash_device *dev, struct spi_bus *bus)
{
	dev->bus = bus;
	dev->sector_size = 4096;
}

/*
 * Write data to flash chip
 */
int spi_flash_write(struct spi_flash_device *dev, uint32_t addr, uint8_t *buf, int count)
{
	int i;

    bb_spi_cs( dev->bus, 1 );
	bb_spi_write( dev->bus, 0x06, 8 );
    bb_spi_cs( dev->bus, 0 );

    bb_spi_delay(dev->bus);

    bb_spi_cs( dev->bus, 1 );
	bb_spi_write(dev->bus, 0x02, 8);
	bb_spi_write(dev->bus, (addr & 0xFF0000) >> 16, 8);
	bb_spi_write(dev->bus, (addr & 0xFF00) >> 8, 8);
	bb_spi_write(dev->bus, (addr & 0xFF), 8);
	for (i = 0; i < count; i++) {
		bb_spi_write(dev->bus, buf[i], 8);
	}
	bb_spi_cs( dev->bus, 0 );

	/* make sure the write is complete */
	while (spi_flash_rsr(dev) & 0x01) {
		/* do nothing */
		}

	return count;
}

/*
 * Read data from flash
 */
int spi_flash_read(struct spi_flash_device *dev, uint32_t addr, uint8_t *buf, int count)
{
	int i;

    bb_spi_cs( dev->bus, 1 );
	bb_spi_write(dev->bus, 0x0b, 8);
	bb_spi_write(dev->bus, (addr & 0xFF0000) >> 16, 8);
	bb_spi_write(dev->bus, (addr & 0xFF00) >> 8, 8);
	bb_spi_write(dev->bus, (addr & 0xFF), 8);
	bb_spi_write(dev->bus, 0, 8);
	for (i = 0; i < count; i++) {
		buf[i] = bb_spi_read(dev->bus, 8);
	}
	bb_spi_cs( dev->bus, 0 );

	return count;
}


/*
 * Sector erase
 */
void spi_flash_erase_sector(struct spi_flash_device *dev, uint32_t addr)
{
    bb_spi_cs( dev->bus, 1 );
	bb_spi_write(dev->bus, 0x06, 8);
	bb_spi_cs( dev->bus, 0 );

    bb_spi_cs( dev->bus, 1 );
	bb_spi_write(dev->bus, 0xD8, 8);
	bb_spi_write(dev->bus, (addr & 0xFF0000) >> 16, 8);
	bb_spi_write(dev->bus, (addr & 0xFF00) >> 8, 8);
	bb_spi_write(dev->bus, (addr & 0xFF), 8);
	bb_spi_cs( dev->bus, 0 );
}

int spi_flash_erase(struct spi_flash_device *dev, uint32_t addr, int count)
{
	int i;
	int sectors = (count + dev->sector_size - 1) / dev->sector_size;

	for (i = 0; i < sectors; ++i) {
		spi_flash_erase_sector(dev, addr + i*dev->sector_size);
		while (spi_flash_rsr(dev) & 0x01)
			;
	}

	return count;
}

#if 0
/*
 * Bulk erase
 */
void
flash_berase(void)
{
	bbspi_transfer(1, 0);
	bbspi_transfer(0, 0x06);
	bbspi_transfer(1, 0);
	bbspi_transfer(0, 0xc7);
	bbspi_transfer(1, 0);
}
#endif

/*
 * Read status register
 */
static uint8_t spi_flash_rsr(struct spi_flash_device *dev)
{
	uint8_t retval;

    bb_spi_cs( dev->bus, 1 );
	bb_spi_write( dev->bus, 0x05, 8);
	retval = bb_spi_read(dev->bus, 8);
    bb_spi_cs( dev->bus, 0 );
	return retval;
}


uint32_t spi_flash_read_id(struct spi_flash_device *dev)
{
    uint32_t val = 0;

    /* make sure the flash is in known state (idle) */
    bb_spi_cs(dev->bus, 1);
    bb_spi_delay(dev->bus);

    bb_spi_cs(dev->bus, 0);
    bb_spi_delay(dev->bus);

    bb_spi_cs(dev->bus, 1);
    bb_spi_write(dev->bus, 0x9f, 8);
    val = (bb_spi_read(dev->bus, 8) << 16);
    val += (bb_spi_read(dev->bus, 8) << 8);
    val += bb_spi_read(dev->bus, 8);
    bb_spi_cs(dev->bus, 0);

    return val;
}



#if 0


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
	return flash_read(offset, buf, count);
}

static int sdb_flash_write(struct sdbfs *fs, int offset, void *buf, int count)
{
	return flash_write(offset, buf, count);
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


#endif