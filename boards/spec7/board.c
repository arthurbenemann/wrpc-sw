#include "board.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "storage.h"
#include <wrc_ptp.h>
#include "spll_defs.h"
#include "spll_common.h"
#include "hw/pps_gen_regs.h"

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
static struct gpio_pin pin_pll_even_odd_n_i  = { &board.gpio_aux, 13 };
static struct gpio_pin pin_pll_sync_done_i   = { &board.gpio_aux, 14 };

#include "configs/ltc6950_defs.h" 
static struct ltc6950_config ltc6950_base_config =
#include "configs/ltc6950_base_config.h" 
static struct ltc6950_config ltc6950_ext_10mhz_config =
#include "configs/ltc6950_ext_10mhz_config.h" 

#define PLL_EVEN_ODD_TIMEOUT_MS 10000
#define PLL_SYNC_TIMEOUT_MS 4000

timeout_t pll_even_odd_timeout;
timeout_t pll_sync_timeout;

//#define CONFIG_HPSEC_GM
#undef CONFIG_HPSEC_GM

//volatile struct softpll_state softpll;

void spec7_set_pll_wr_mode(int wrc_ptp_mode)
{
    int pll_wr_mode;
    int mode_hpsec_gm = 0;

    // Set clock multiplexers (U63, U64) depending on wrc_ptp_mode
    switch(wrc_ptp_mode) {
        case WRC_MODE_GM || WRC_MODE_ABSCAL:
            // Default reference design locks local VCXO to external 10 MHz
            pll_wr_mode = PLL_WR_MODE_SLAVE;
#if defined(CONFIG_HPSEC_GM)
            // When HPSEC is used in GM mode then HPSEC locks to external
            // 10 MHz via LTC6950 and WRC_MODE *must* be free running master.
            // Note: WRC_MODE_GM tries to align the local VCXO with the
            // external 10 MHz but in HPSEC_GM case the VCXO is not used.
            pp_printf("ERROR: HPSEC_GM must use WRC_MODE_MASTER.\n");
#endif
            break;
        case WRC_MODE_MASTER:
            pll_wr_mode = PLL_WR_MODE_MASTER;
#if defined(CONFIG_HPSEC_GM)
            // When HPSEC is used in GM mode then HPSEC locks to external
            // 10 MHz via LTC6950 and WRC_MODE *must* be free running master.
            pll_wr_mode = PLL_WR_MODE_GM;
            mode_hpsec_gm = 1;
#endif
            break;
        default:
            pll_wr_mode = PLL_WR_MODE_SLAVE;
    }

    gen_gpio_out( &pin_pll_wr_mode0_o, (pll_wr_mode & 0x1) ? 1 : 0);
    gen_gpio_out( &pin_pll_wr_mode1_o, (pll_wr_mode & 0x2) ? 1 : 0);
    board_dbg("wr_mode: %d\n", pll_wr_mode);

    // ltc6950 initialization depending on wrc_ptp_mode
    board_dbg("Initialize ltc6950.\n");
    if (wrc_ptp_mode == WRC_MODE_MASTER | mode_hpsec_gm == 1) {
        // 10 MHZ from TCXO (WRC_MODE_MASTER) on outputs 0, 1, 2
        // or (for HPSEC in WRC_MODE GM) External 10 MHZ In (Bulls-Eye B03/B04) => 125 MHz
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_ext_10mhz_config);
        while ((ltc6950_read(&board.ltc6950_pll,  0x00) & LTC6950_LOCK) == 0);
        board_dbg("ltc6950 locked.\n");
#if defined(CONFIG_HPSEC_GM)
        pll_sync();
#endif
    } else {
        // Forward 125 MHz VCXO_REFCLK at CLK input to outputs 0, 1, 2
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_base_config);
    }

    board_dbg("Select ltc6950 output as clk_sys source.\n");
    /* ltc6950 now initialized so switch clk_sys from free running clk_dmtd to ltc6950 output */
    gen_gpio_out( &pin_pll_clk_sel, 1);
    timer_delay_ms(10);
}

int pll_sync()
{
    // Used in HPSEC Grand Master mode where external 10MHz generates 125MHz.
    // 125MHz is not an integer multiple of 10MHz so it has two lock modes: even/odd.
    // The generated 125MHz must be even/odd alligned with the external 10MHz/1PPS.
    
    tmo_init(&pll_even_odd_timeout, PLL_EVEN_ODD_TIMEOUT_MS);
    while (gen_gpio_in( &pin_pll_even_odd_n_i ) == 0) {
        // Reset the PLL (RES6950 clears itself)
        board_dbg("Reset ltc6950...\n");
        ltc6950_write( &board.ltc6950_pll, 0x03, 4);
        timer_delay_ms(1);
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_ext_10mhz_config);
        while ((ltc6950_read(&board.ltc6950_pll,  0x00) & LTC6950_LOCK) == 0);
        timer_delay_ms(1000);  // wait for next PPS
        if ( tmo_expired(&pll_even_odd_timeout)) {
            pp_printf("TIMEOUT: External 10MHz/1PPS lock to \"even\" 125MHz clock cycle.\n");
            return 0;
        }
    }
    board_dbg("HPSEC_GM mode: External 10MHz/1PPS lock achieved on \"even\" 125MHz clock cycle\n");

    // Trigger a clk_ref_125m to clk_ref_62m5 divider synchronisation
    gen_gpio_out( &pin_pll_sync_o, 1);
    gen_gpio_out( &pin_pll_sync_o, 0);

    tmo_init(&pll_sync_timeout, PLL_SYNC_TIMEOUT_MS);
    // Wait for sync sequence done
    while (gen_gpio_in( &pin_pll_sync_done_i ) == 0) {
        if ( tmo_expired(&pll_sync_timeout)) {
            pp_printf("TIMEOUT: clk_ref_125m to clk_ref_62m5 divider synchronization.\n");
            return 0;
        }
    }
    board_dbg("HPSEC_GM mode: clk_ref_125m to clk_ref_62m5 divider synchronization done\n");

/*
    PPSG->ESCR = PPSG_ESCR_SYNC;
    tmo_init(&pll_sync_timeout, PLL_SYNC_TIMEOUT_MS);
    // Wait for PPS sync sequence done
    while (PPSG->ESCR & PPSG_ESCR_SYNC == 0) {
        if ( tmo_expired(&pll_sync_timeout)) {
            pp_printf("TIMEOUT: External PPS alignment.\n");
            return 0;
        }
    }
    board_dbg("HPSEC_GM mode: synced to external PPS.\n");
*/
    phy_calibration_init();
    while (!phy_calibration_done()) {
        phy_calibration_poll();
    }

    return 1;
}

int post_pll_lock(int wrc_ptp_mode)
{
#if defined(CONFIG_HPSEC_GM)
    // Sync external PPS when in HPSEC_GM mode
    PPSG->ESCR = PPSG_ESCR_SYNC;
    tmo_init(&pll_sync_timeout, PLL_SYNC_TIMEOUT_MS);
    // Wait for PPS sync sequence done
    while (PPSG->ESCR & PPSG_ESCR_SYNC == 0) {
        if ( tmo_expired(&pll_sync_timeout)) {
            pp_printf("TIMEOUT: External PPS alignment.\n");
            return 0;
        }
    }
    board_dbg("HPSEC_GM mode: synced to external PPS.\n");
#endif

    if (wrc_ptp_mode == WRC_MODE_MASTER)
        //phy_calibration_init();

    return 1;
}


int spec7_init()
{
    /* most of the I/Os of the slow peripherals (i2c, spi) are bitbanged. First, let's
       initialize the GPIO controller they're connected to */
    wb_gpio_create( &board.gpio_aux, BASE_GPIO );

    // Use free running dmtd clock for bootstrapping
    gen_gpio_out( &pin_pll_clk_sel, 0);
    board_dbg("Use free running dmtd clock for bootstrapping.\n");

    // PLL reset (although not connected at top level) de-asserted
    gen_gpio_out( &pin_pll_reset_n_o, 1);

    // Do not (yet) sync the clk_ref_125m to clk_ref_62m5 divider
    gen_gpio_out( &pin_pll_sync_o, 0);

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
    timer_delay_ms(1);

    int id = ltc6950_read( &board.ltc6950_pll, 0x16 );
    if( id != 0x65 )
    {
        board_dbg("detect LTC6950: ID %x should be %x\n", id, 0x65 );
    } else {
        spec7_set_pll_wr_mode(WRC_MODE_SLAVE);
    }

    return 0;
}

int wrc_board_early_init()
{
    spec7_init();
    return 0;
}

struct i2c_bus            dev_i2c_eeprom;
struct i2c_eeprom_device  wrc_eeprom_device;

int wrc_board_init()
{
    // int memtype;
    // uint32_t sdbfs_entry;
    // uint32_t sector_size;

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
    bb_i2c_create(&dev_i2c_eeprom,
         &pin_eeprom_scl,
         &pin_eeprom_sda );
    bb_i2c_init(&dev_i2c_eeprom);

    i2c_eeprom_create(&wrc_eeprom_device, &dev_i2c_eeprom, 0x50, 2);
    storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_device );

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

extern int phy_calibration_poll();
extern void phy_calibration_init();

int wrc_board_create_tasks()
{
    wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );

    return 0;
}
