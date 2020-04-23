#include "board.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/syscon.h"
#include "storage.h"

int wrc_board_early_init()
{
	return 0;
}

int wrc_board_init()
{
	int memtype;
	uint32_t sdbfs_addr;
	uint32_t sector_size;

	/*
	 * declare GPIO pins and configure their directions for bit-banging SPI
	 * limit SPI speed to 10MHz by setting bit_delay = CPU_CLOCK / 10^6
	 */
	bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, CPU_CLOCK / 10000000 );

	spi_wrc_flash.rd_falling_edge = 1;

	/*
	 * Read from gateware info about used memory. Currently only base
	 * address and sector size for memtype flash is supported.
	 */
	get_storage_info(&memtype, &sdbfs_addr, &sector_size);

	/*
	 * Initialize SPI flash and read its ID
	 */
	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, sector_size);

	/*
	 * Initialize storage subsystem with newly created SPI Flash
	 */
	storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );

	/*
	 * Mount SDBFS filesystem from storage.
	 */
	storage_mount( &wrc_storage_dev );
	return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
