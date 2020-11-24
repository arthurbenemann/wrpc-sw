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

#define CONFIG_HPSEC_GM
//#undef CONFIG_HPSEC_GM

//volatile struct softpll_state softpll;

void spec7_set_pll_wr_mode(int wrc_ptp_mode)
{
    int pll_wr_mode;

#if defined(CONFIG_HPSEC_GM)
    // When HPSEC is used then (Morion MV336) 10MHz is tuned
    // and the LTC6950 generates 125 MHz.
    pll_wr_mode = PLL_WR_MODE_GM;
#else
    // When referecne design is used then (Morion MV336) 10MHz is tuned
    // Set clock multiplexers (U63, U64) depending on wrc_ptp_mode
    switch(wrc_ptp_mode) {
        case WRC_MODE_GM || WRC_MODE_ABSCAL:
            // Default reference design locks local VCXO to external 10 MHz
            pll_wr_mode = PLL_WR_MODE_SLAVE;
            break;
        case WRC_MODE_MASTER:
            pll_wr_mode = PLL_WR_MODE_MASTER;
            break;
        default:
            pll_wr_mode = PLL_WR_MODE_SLAVE;
    }
#endif

    gen_gpio_out( &pin_pll_wr_mode0_o, (pll_wr_mode & 0x1) ? 1 : 0);
    gen_gpio_out( &pin_pll_wr_mode1_o, (pll_wr_mode & 0x2) ? 1 : 0);
    board_dbg("wr_mode: %d\n", pll_wr_mode);

    // ltc6950 initialization depending on wrc_ptp_mode
    board_dbg("Initialize ltc6950.\n");
    if (pll_wr_mode == PLL_WR_MODE_MASTER | pll_wr_mode == PLL_WR_MODE_GM) {
        // 10 MHZ from TCXO (WRC_MODE_MASTER) on outputs 0, 1, 2
        // or (for HPSEC in WRC_MODE GM) External 10 MHZ In (Bulls-Eye B03/B04)
        // via LTC6950 => 125 MHz
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_ext_10mhz_config);
        while ((ltc6950_read(&board.ltc6950_pll,  0x00) & LTC6950_LOCK) == 0);
        board_dbg("ltc6950 locked.\n");
    } else {
        // Forward 125 MHz VCXO_REFCLK at CLK input to outputs 0, 1, 2
        ltc6950_configure(&board.ltc6950_pll, &ltc6950_base_config);
    }

    board_dbg("Select ltc6950 output as clk_sys source.\n");
    /* ltc6950 now initialized so switch clk_sys from free running clk_dmtd to ltc6950 output */
    gen_gpio_out( &pin_pll_clk_sel, 1);
    timer_delay_ms(10);
}


int post_pll_lock(int wrc_ptp_mode)
{
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

struct i2c_bus            dev_i2c_eeprom;
struct i2c_eeprom_device  wrc_eeprom_dev;
struct i2c_eeprom_device  wrc_uid_dev;

int wrc_board_early_init()
{
    /* Initialize SPEC7 clocking */
    spec7_init();

    /* create and initialize eeprom I2C bus */
    bb_i2c_create(&dev_i2c_eeprom,
         &pin_eeprom_scl,
         &pin_eeprom_sda );
    bb_i2c_init(&dev_i2c_eeprom);

    i2c_eeprom_create(&wrc_eeprom_dev, &dev_i2c_eeprom, EEPROM_ADR, 2);
    storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_dev );

    /*
     * Mount SDBFS filesystem from storage.
     */
    storage_mount( &wrc_storage_dev );

    /* create and initialize UID eeprom I2C bus */
    i2c_eeprom_create(&wrc_uid_dev, &dev_i2c_eeprom, UID_EEPROM_ADR, 1);

    return 0;
}

int wrc_board_init()
{
	uint8_t mac_addr[6];
	/*
	 * Read MAC addr from Unique-ID, IC D12, 24AA025E48
	 */

	i2c_eeprom_read(&wrc_uid_dev, UID_OFFSET , mac_addr, sizeof(mac_addr));
	board_dbg("MAC addr: %x:%x:%x:%x:%x:%x\n",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

	ep_set_mac_addr(&wrc_endpoint_dev, mac_addr);
	ep_pfilter_init_default(&wrc_endpoint_dev);

    return 0;
}

extern int phy_calibration_poll();
extern void phy_calibration_init();

int wrc_board_create_tasks()
{
    wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );

    return 0;
}
