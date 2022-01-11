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
#include "hw/sit5359_regs.h"

//struct spec7_board board;

struct wr_sit5359_interface_device
{
	void *base_addr;
	uint8_t i2c_addr;
	struct gpio_pin pin_scl;
	struct gpio_pin pin_sda;
	struct gpio_device gpio_i2c;
	struct i2c_bus master;
	int pull_range, hsdiv;
	uint64_t rfreq;
};

#define SIT5359_PIN_SCL 0
#define SIT5359_PIN_SDA 1

// I2C_ADDR (A0=0) => 1100010 (WR:0xC4, RD:0xC5 or 0x62+r/w)
// I2C_ADDR (A0=1) => 1101010 (WR:0xD4, RD:0xD5 or 0x6A+r/w)
#define SIT5359_I2C_ADDR 0x62

struct
{
    struct gpio_device gpio_aux;
    struct spi_bus spi_ltc6950;
    struct ltc6950_device ltc6950_pll;
    struct wr_sit5359_interface_device sit5359;

    int pll_wr_mode;
} board;

static void sit5359_gpio_out(const struct gpio_pin *pin, int value)
{
	struct wr_sit5359_interface_device* dev = ( struct wr_sit5359_interface_device* ) pin->device->priv;

	

	uint32_t mask = (pin->pin == SIT5359_PIN_SCL ? SIT5359_GPCR_SCL : SIT5359_GPCR_SDA );
	uint32_t reg = (value ? SIT5359_REG_GPSR : SIT5359_REG_GPCR );


	writel( mask, dev->base_addr + reg );
}


static void sit5359_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
	sit5359_gpio_out(pin, !dir);
}


static int sit5359_gpio_in(const struct gpio_pin *pin)
{
	struct wr_sit5359_interface_device* dev = ( struct wr_sit5359_interface_device* ) pin->device->priv;

	uint32_t gpsr = readl( dev->base_addr + SIT5359_REG_GPSR );

	if ( pin->pin == SIT5359_PIN_SCL )
		return (gpsr & SIT5359_GPSR_SCL ? 1 : 0);
	else
		return (gpsr & SIT5359_GPSR_SDA ? 1 : 0);
}

static void wr_sit5359_interface_init( struct wr_sit5359_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr )
{

	dev->base_addr = (void *) base_addr;
	dev->gpio_i2c.priv = (void *) dev;
	dev->gpio_i2c.read_pin = sit5359_gpio_in;
	dev->gpio_i2c.set_dir = sit5359_gpio_set_dir;
	dev->gpio_i2c.set_out = sit5359_gpio_out;
	dev->i2c_addr = i2c_addr;
	dev->pin_scl.device = &dev->gpio_i2c;
	dev->pin_scl.pin = SIT5359_PIN_SCL;
	dev->pin_sda.device = &dev->gpio_i2c;
	dev->pin_sda.pin = SIT5359_PIN_SDA;
	bb_i2c_create( &dev->master, &dev->pin_scl, &dev->pin_sda );
}

static void sit5359_read( struct wr_sit5359_interface_device *dev, uint8_t addr, uint8_t *data, int count )
{
	int i;

	bb_i2c_start( &dev->master );
	bb_i2c_put_byte( &dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( &dev->master, addr );
	bb_i2c_repeat_start( &dev->master );
	bb_i2c_put_byte( &dev->master, (dev->i2c_addr << 1) | 1 );

	for(i = 0; i < count; i ++)
		bb_i2c_get_byte( &dev->master, &data[i], i == (count - 1) ? 1 : 0 );

	bb_i2c_stop( &dev->master );
}


static void sit5359_i2c_write( struct wr_sit5359_interface_device *dev, uint8_t addr, uint8_t *data, int count )
{
	int i;

	bb_i2c_start( &dev->master );
	bb_i2c_put_byte( &dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( &dev->master, addr );
	
	for(i = 0; i < count; i ++)
	{
		bb_i2c_put_byte( &dev->master, data[i] );
	}

	bb_i2c_stop( &dev->master );
}

static int sit5359_dev_init( struct wr_sit5359_interface_device *dev )
{
    // Enable SPLL and Osc Output Enable
    // I2C bus freqency = 1/(4*(30+1)*16ns) = 504 KHz
    writel( SIT5359_CR_SPLL_EN | SIT5359_CR_OSC_OE | SIT5359_CR_CLK_DIV_W(30) | SIT5359_CR_I2C_ADDR_W ( ( dev->i2c_addr << 1 ) ), dev->base_addr + SIT5359_REG_CR );

	return 0;
}

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
static struct gpio_pin pin_aux_scl           = { &board.gpio_aux, 15 };
static struct gpio_pin pin_aux_sda           = { &board.gpio_aux, 16 };

#include "configs/ltc6950_defs.h" 
static struct ltc6950_config ltc6950_base_config =
#include "configs/ltc6950_base_config.h" 
static struct ltc6950_config ltc6950_ext_10mhz_config =
#include "configs/ltc6950_ext_10mhz_config.h" 

static spll_gain_schedule_t spll_main_ocxo_gain_sched;

#define PLL_EVEN_ODD_TIMEOUT_MS 10000
#define PLL_SYNC_TIMEOUT_MS 4000

timeout_t pll_even_odd_timeout;
timeout_t pll_sync_timeout;

//volatile struct softpll_state softpll;

static void spec7_spll_setup(void)
{
/* configure a suitable PI gain schedule for the SoftPLL: */
    spll_gain_schedule_t* gs=  &spll_main_ocxo_gain_sched;

/* we start with ~100 Hz bandwidth to make it lock reasonably fast */
    gs->stages[0].kp = -5500;
    gs->stages[0].ki = -30;
    gs->stages[0].lock_samples = 30000;
    gs->stages[0].shift = 12;

/* once it's locked, the loop bandwidth is switched to ~0.1 Hz to filter out WR link added phase noise */
    gs->stages[1].kp = -3000;
    gs->stages[1].ki = -5;
    gs->stages[1].lock_samples = 10000;
    gs->stages[1].shift = 16;

#if defined(CONFIG_TARGET_HPSEC)
    gs->n_stages = 2;   // 2 stages: SPEC7 Crysteck => HPSEC Morion MV336
	spll_set_gain_schedule( gs );
#else
    gs->n_stages = 1;   // 1 stage: SPEC7 Crysteck
	//spll_set_gain_schedule( gs );  // Repair: Gain schedule keeps restarting in mode gm
#endif

}

void spec7_set_pll_wr_mode(int wrc_ptp_mode)
{
    int pll_wr_mode;
    int mode_hpsec_gm = 0;

    // Set clock multiplexers (U63, U64) depending on wrc_ptp_mode
    switch(wrc_ptp_mode) {
        case WRC_MODE_GM || WRC_MODE_ABSCAL:
            // Default reference design locks local VCXO to external 10 MHz
            pll_wr_mode = PLL_WR_MODE_SLAVE;
#if defined(CONFIG_TARGET_HPSEC)
            // When HPSEC is used in GM mode then HPSEC locks to external
            // 10 MHz via LTC6950 and WRC_MODE *must* be free running master.
            // Note: WRC_MODE_GM tries to align the local VCXO with the
            // external 10 MHz but in HPSEC_GM case the VCXO is not used.
            pp_printf("ERROR: HPSEC_GM must use WRC_MODE_MASTER.\n");
#endif
            break;
        case WRC_MODE_MASTER:
            pll_wr_mode = PLL_WR_MODE_MASTER;
#if defined(CONFIG_TARGET_HPSEC)
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

    /* Setup the SoftPLL for the OCXO we have */
    spec7_spll_setup();

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
struct i2c_bus            dev_i2c_aux;
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

    /* create and initialize auxiliary I2C bus */
    bb_i2c_create(&dev_i2c_aux,
         &pin_aux_scl,
         &pin_aux_sda );
    bb_i2c_init(&dev_i2c_aux);

    i2c_eeprom_create(&wrc_eeprom_dev, &dev_i2c_eeprom, EEPROM_ADR, 2);
    storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_dev );

    /*
     * Mount SDBFS filesystem from storage.
     */
    storage_mount( &wrc_storage_dev );

    /* create and initialize UID eeprom I2C bus */
    i2c_eeprom_create(&wrc_uid_dev, &dev_i2c_eeprom, UID_EEPROM_ADR, 1);

	wr_sit5359_interface_init( &board.sit5359, (void *) BASE_SIT5359_INTERFACE, SIT5359_I2C_ADDR );

    return 0;
}

int wrc_board_init()
{
	int i;
	uint32_t rfreq;
	uint8_t regs[6];

    // set I2C bus speed and OSC Output enable
    sit5359_dev_init(&board.sit5359);

    regs[4] = 0x00; // SiT5339 Reg 0x02 15:8
    regs[5] = 0x03; // SiT5339 Reg 0x02 7:0  => Pull Range 25 ppm
    // write SiT5359 0x02 with Pull Range (2 bytes)
    board_dbg("SiT5359: Wr Pull Range, reg 0x2\n");
    sit5359_i2c_write(&board.sit5359, 0x02, regs + 4, 2 );

    board_dbg("SiT5359: RD All regs\n");
    // read back all SiT5359 registers (6 bytes)
    sit5359_read(&board.sit5359, 0x00, regs, 6 );
    board_dbg("SiT5359 Regs: %x:%x:%x:%x:%x:%x\n",regs[0],regs[1],regs[2],regs[3],regs[4],regs[5]);

/*
    // loop through frequency control word
    for(i = 0; i < 65536; i ++)
    {
        rfreq = (i-32768)<<9;
        board_dbg("rfreq: %d\n",rfreq);
        regs[0]= (rfreq & 0x0000ff00)>>8;
        regs[1]= (rfreq & 0x000000ff);
        regs[2]= (rfreq & 0xff000000)>>24 | 0x04;  // Or OE!
        regs[3]= (rfreq & 0x00ff0000)>>16;
        board_dbg("SiT5359 FreqCntrl: %x:%x:%x:%x\n",regs[0],regs[1],regs[2],regs[3]);
        sit5359_i2c_write(&board.sit5359, 0x00, regs, 4 );
    }
*/
    
    // Sent some phoney I2C words, just to test I2C aux interface
	bb_i2c_start( &dev_i2c_aux );
	bb_i2c_put_byte(&dev_i2c_aux, 0xA0);
	bb_i2c_put_byte(&dev_i2c_aux, 0x00);
	bb_i2c_put_byte(&dev_i2c_aux, 0xA1);
	bb_i2c_stop(&dev_i2c_aux);

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
