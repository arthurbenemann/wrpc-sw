#include "board.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "storage.h"

struct spec7_board board;

static struct gpio_pin pin_pll_cs_n_o        = { &board.gpio_aux, 0 };
static struct gpio_pin pin_pll_mosi_o        = { &board.gpio_aux, 1 };
static struct gpio_pin pin_pll_miso_i        = { &board.gpio_aux, 2 };
static struct gpio_pin pin_pll_sck_o         = { &board.gpio_aux, 3 };
static struct gpio_pin pin_pll_reset_n_o     = { &board.gpio_aux, 4 };
static struct gpio_pin pin_pll_lock_i        = { &board.gpio_aux, 5 };
static struct gpio_pin pin_pll_status_i      = { &board.gpio_aux, 6 };
static struct gpio_pin pin_pll_sync_o        = { &board.gpio_aux, 7 };
static struct gpio_pin pin_pll_wr_mode0_o    = { &board.gpio_aux, 8 };
static struct gpio_pin pin_pll_wr_mode1_o    = { &board.gpio_aux, 9 };
static struct gpio_pin pin_pll_clk_sel       = { &board.gpio_aux, 10 };
static struct gpio_pin pin_eeprom_scl        = { &board.gpio_aux, 11 };
static struct gpio_pin pin_eeprom_sda        = { &board.gpio_aux, 12 };

static struct ltc6950_config ltc6950_base_config =
#include "configs/ltc6950_base_config.h" 
static struct ltc6950_config ltc6950_ext_10mhz_config =
#include "configs/ltc6950_ext_10mhz_config.h" 

//int pll_wr_mode = PLL_WR_MODE_MASTER;
int pll_wr_mode = PLL_WR_MODE_SLAVE;
//int pll_wr_mode = PLL_WR_MODE_GM;

void spec7_set_pll_wr_mode(int pll_wr_mode)
{
    gen_gpio_out( &pin_pll_wr_mode0_o, (pll_wr_mode & 0x1) ? 1 : 0);
    gen_gpio_out( &pin_pll_wr_mode1_o, (pll_wr_mode & 0x2) ? 1 : 0);
}

int spec7_init()
{
    // Use free running dmtd clock for bootstrapping
    gen_gpio_out( &pin_pll_clk_sel, 0);

    // PLL reset and sync de-asserted
    gen_gpio_out( &pin_pll_sync_o, 0);
    gen_gpio_out( &pin_pll_reset_n_o, 1);

    /* initialize the SPI bus for the SPEC7 PLL (LTC6950 U66) */
    bb_spi_create( &board.spi_ltc6950,
        &pin_pll_cs_n_o,
        &pin_pll_mosi_o,
        &pin_pll_miso_i,
        &pin_pll_sck_o,
        100 );

    ltc6950_init(&board.ltc6950_pll, &board.spi_ltc6950);

    // Reset the PLL (RES6950 clears itself)
    ltc6950_write( &board.ltc6950_pll, 0x03, 4);
    int id = ltc6950_read( &board.ltc6950_pll, 0x16 );
    if( id != 0x65 )
    {
        board_dbg("detect LTC6950: ID %x should be %x\n", id, 0x65 );
    } else {
        // Configuration for the SPEC7: Forward 125 MHz VCXO_REFCLK at CLK input to outputs 0, 1, 2
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_base_config);
        // Set clock multiplexers (U63, U64) depending on WR mode
        spec7_set_pll_wr_mode(pll_wr_mode);
    }

    //while ((ltc6950_read( &board.ltc6950_pll, 0x16 ) &4) == 0);

    board_dbg("Switch clk_sys source from free running clk_dmtd to ltc6950 output.\n");
    /* ltc6950 now initialized so switch clk_sys from free running clk_dmtd to ltc6950 output */
    gen_gpio_out( &pin_pll_clk_sel, 1);
    timer_delay(1000);
    board_dbg("now running on ref clock.\n");

    return 0;
}

int wrc_board_early_init()
{
    spec7_init();
    return 0;
}

int wrc_board_init()
{
    // int memtype;
    // uint32_t sdbfs_entry;
    // uint32_t sector_size;
    struct i2c_bus            bus_i2c_eeprom;
    struct i2c_eeprom_device  dev_i2c_eeprom;

    /*
     * declare GPIO pins and configure their directions for bit-banging SPI
     * limit SPI speed to 10MHz by setting bit_delay = CPU_CLOCK / 10^6
     */
    //bb_spi_create( &spi_wrc_flash,
    //    &pin_sysc_spi_ncs,
    //    &pin_sysc_spi_mosi,
    //    &pin_sysc_spi_miso,
    //    &pin_sysc_spi_sclk, CPU_CLOCK / 10000000 );
        
    //spi_wrc_flash.rd_falling_edge = 1;

    /* create and initialize eeprom I2C bus */
    bb_i2c_create(&bus_i2c_eeprom,
         &pin_eeprom_scl,
         &pin_eeprom_sda );
    bb_i2c_init(&bus_i2c_eeprom);

    i2c_eeprom_create(&dev_i2c_eeprom, &bus_i2c_eeprom, 0x50, 0x00);

    storage_i2c_eeprom_create( &wrc_storage_dev, &dev_i2c_eeprom );

    /*
     * Read from gateware info about used memory. Currently only base
     * address and sector size for memtype flash is supported.
     */
    // get_storage_info(&memtype, &sdbfs_entry, &sector_size);

    /*
     * Initialize SPI flash and read its ID
     */
    // spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, sector_size, sdbfs_entry);

    /*
     * Initialize storage subsystem with newly created SPI Flash
     */
    // storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );

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
