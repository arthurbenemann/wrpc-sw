#include "board.h"
#include "dev/bb_spi.h"
#include "dev/bb_i2c.h"
#include "dev/spi_flash.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "storage.h"

int wrc_board_early_init()
{
	int memtype;
	uint32_t sdbfs_entry;
	uint32_t sector_size;
	uint8_t mac_addr[6];


	if (EEPROM_STORAGE) {
	/* EEPROM support */
		bb_i2c_create( &i2c_wrc_eeprom,
			&pin_sysc_fmc_scl,
			&pin_sysc_fmc_sda );
		bb_i2c_init( &i2c_wrc_eeprom );

		i2c_eeprom_create( &wrc_eeprom_dev, &i2c_wrc_eeprom, FMC_EEPROM_ADR, 2);
		storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_dev );
	} else {
	/* Flash support */
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
		get_storage_info(&memtype, &sdbfs_entry, &sector_size);

		/*
		 * Initialize SPI flash and read its ID
		 */
		spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, sector_size, sdbfs_entry);

		/*
		 * Initialize storage subsystem with newly created SPI Flash
		 */
		storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
	}

	/*
	 * Mount SDBFS filesystem from storage.
	 */
	storage_mount( &wrc_storage_dev );

	/*
	 * Try reading MAC addr stored in flash
	 */
	if (storage_get_persistent_mac(mac_addr) == -1) {
		board_dbg("Failed to get MAC address from the flash. Using fallback address.\n");
		mac_addr[0] = 0x22;
		mac_addr[1] = 0x33;
		mac_addr[2] = 0x44;	/* fallback MAC if get_persistent_mac fails */
		mac_addr[3] = 0x55;
		mac_addr[4] = 0x66;
		mac_addr[5] = 0x77;
	}
	ep_set_mac_addr(mac_addr);

	return 0;
}

int wrc_board_init()
{

	return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
