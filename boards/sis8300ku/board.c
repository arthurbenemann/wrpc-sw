#include "board.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/syscon.h"
#include "softpll_ng.h"
#include "storage.h"
#include <wrc-event.h>

extern  uint32_t sdbfs_default_bin[] = 
{
	#include "sdbfs-image.h"
};

static const int32_t flash_entry_points[] = { 0x0f00000, -1 };

int wrc_board_early_init()
{
	int memtype;
	uint32_t sdbfs_entry;

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
	 * Initialize SPI flash and read its ID
	 */
	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, 0x10000, 0xf00000);
	wrc_flash_dev.use_4byte_addr = 0;
	wrc_flash_dev.size = 0x1000000; // 32 MB flash

	uint32_t id = spi_flash_read_id( &wrc_flash_dev );

	if( id != 0x00012018 && id != 0xC22019 )
	{
		pp_printf("Can't find a matching flash memory. Read ID = 0x%08x\n", id);
		return 0;
	}

	/*
	 * Initialize storage subsystem with newly created SPI Flash
	 */
	storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );

	// override default entry point for the flash
	wrc_storage_dev.entry_points = flash_entry_points;

	/*
	 * Mount SDBFS filesystem from storage.
	 */
	storage_mount( &wrc_storage_dev );

	// fixme: read MAC address from the MMC
    uint8_t mac[6];

	storage_get_persistent_mac(mac);

	board_dbg("Board MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ep_set_mac_addr( mac );

	spll_set_aux_mode( 0, SPLL_AUX_MODE_TRACKING_SOURCE );
	spll_set_aux_mode( 1, SPLL_AUX_MODE_TRACKING_SOURCE );
	
	return 0;
}



static int sis83k_handle_event( int event )
{
	if ( event == WRC_EVENT_LINK_DOWN )
	{
		// fixme: do we need forced PHY reset here?
	}
}

int wrc_board_init()
{
	event_handler_register( 1 << WRC_EVENT_LINK_DOWN, 1, sis83k_handle_event );

	return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
