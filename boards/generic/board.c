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
	bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, 10 );

	spi_wrc_flash.rd_falling_edge = 1;

	get_storage_info(&memtype, &sdbfs_addr, &sector_size);
	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, sector_size);

	storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
	storage_mount( &wrc_storage_dev );
	return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
