#include "board.h"

#include "dev/syscon.h"
#include "dev/i2c.h"
#include "dev/gpio.h"
#include "dev/bb_i2c.h"
#include "dev/clock_monitor.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"

#include "hw/si570_if_wb.h"

#include "storage.h"

struct wr_si57x_interface_device
{
	void *base_addr;
	uint8_t i2c_addr;
	struct gpio_pin pin_scl;
	struct gpio_pin pin_sda;
	struct gpio_device gpio_i2c;
	struct i2c_bus master;
	int n1, hsdiv;
	uint64_t rfreq;
};

struct idt8v_clock_mux_device {
	struct i2c_bus *bus;
	uint8_t i2c_addr;
	uint8_t regs[16];
};

#define SI57X_PIN_SCL 0
#define SI57X_PIN_SDA 1

#define SI57X_I2C_ADDR 0x55
#define IDT8V_I2C_ADDR 0x58

struct pca9554_gpio_device
{
	struct i2c_bus *bus;
	uint8_t i2c_addr;
	struct gpio_device gpio;
};


struct {
	struct gpio_device gpio_aux;
	struct wr_si57x_interface_device si57x;
	struct wb_clock_monitor_device clk_mon;
	struct idt8v_clock_mux_device clk_mux;
	struct pca9554_gpio_device gpio_rtm_main;
	struct pca9554_gpio_device gpio_rtm_sfp;
	struct i2c_eeprom_device mac_eeprom;
} board;

static void idt8v_read_regs( struct idt8v_clock_mux_device*dev )
{
	int i;
	bb_i2c_start( dev->bus );
	bb_i2c_put_byte( dev->bus, (dev->i2c_addr << 1) | 1 );
	for( i = 0; i < 16; i++ )
		bb_i2c_get_byte( dev->bus, &dev->regs[i], i == 15 ? 1 : 0 );
	bb_i2c_stop( dev->bus );
}

static void idt8v_commit_configuration( struct idt8v_clock_mux_device*dev )
{
	int i;
	bb_i2c_start( dev->bus );
	bb_i2c_put_byte( dev->bus, (dev->i2c_addr << 1) );
	for( i = 0; i < 16; i++ )
		bb_i2c_put_byte( dev->bus, dev->regs[i] );
	bb_i2c_stop( dev->bus );
}


static void idt8v_clock_mux_init ( struct idt8v_clock_mux_device*dev, struct i2c_bus* bus, uint8_t i2c_addr )
{
	dev->bus = bus;
	dev->i2c_addr = i2c_addr;

	idt8v_read_regs( dev );
}



static void idt8v_configure_io ( struct idt8v_clock_mux_device*dev, int io_index, int is_input, int term_en, int output_sel )
{
	uint8_t r = 0;

	if( is_input )
	{
		r = term_en ? (1<<6) : 0;
	}
	else
	{
		r = (1 << 7) | output_sel;
	}

	dev->regs[ io_index ] = r;
}



static void si57x_gpio_out(const struct gpio_pin *pin, int value)
{
	struct wr_si57x_interface_device* dev = ( struct wr_si57x_interface_device* ) pin->device->priv;

	

	uint32_t mask = (pin->pin == SI57X_PIN_SCL ? SI570_GPCR_SCL : SI570_GPCR_SDA );
	uint32_t reg = (value ? SI570_REG_GPSR : SI570_REG_GPCR );


	writel( mask, dev->base_addr + reg );
}


static void si57x_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
	si57x_gpio_out(pin, !dir);
}


static int si57x_gpio_in(const struct gpio_pin *pin)
{
	struct wr_si57x_interface_device* dev = ( struct wr_si57x_interface_device* ) pin->device->priv;

	uint32_t gpsr = readl( dev->base_addr + SI570_REG_GPSR );

	if ( pin->pin == SI57X_PIN_SCL )
		return (gpsr & SI570_GPSR_SCL ? 1 : 0);
	else
		return (gpsr & SI570_GPSR_SDA ? 1 : 0);
}

void tca9548_select_channels( struct i2c_bus *bus, uint8_t tca_address, uint8_t channel_mask )
{
	bb_i2c_start( bus );
	bb_i2c_put_byte( bus, tca_address << 1 );
	bb_i2c_put_byte( bus, channel_mask );
	bb_i2c_stop( bus );
}

void si57x_read( struct wr_si57x_interface_device *dev, uint8_t addr, uint8_t *data, int count )
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


void si57x_write( struct wr_si57x_interface_device *dev, uint8_t addr, uint8_t *data, int count )
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

void si57x_get_xtal_frequency( struct wr_si57x_interface_device *dev, uint32_t* freq_hz )
{
	uint8_t regs[16];

	si57x_read( &board.si57x, 7, regs, 9 ); // R7... R15

	uint64_t rfreq = ( (uint64_t) regs[12-7] ) | // R12
					 ( ( (uint64_t) regs[11-7]) << 8 ) | // R11
					 ( ( (uint64_t) regs[10-7]) << 16 ) | // R10
					 ( ( (uint64_t) regs[9-7]) << 24 ) | // R9
					 ( ( (uint64_t) regs[8-7] & 0x3f) << 32 ); // R8

	uint64_t n1 = ( ( (regs[0] & 0x1f) << 2) | (regs[1] >> 6) ) + 1;
	uint64_t hs_div = (regs[0] >> 5) + 4;

	board_dbg("Si57x: RFREQ %08x %08x n1 %d hsdiv %d\n", (uint32_t) (rfreq >> 32), (uint32_t) rfreq, (int)n1, (int)hs_div );

	if( rfreq == 0 )
	{
		board_dbg("strange, rfreq == 0\n");
		return;
	}

	uint64_t f0 = 100000000;
	uint64_t f_xtal = (f0 * hs_div * n1 ) * ( 1ULL << 28 ) / rfreq;

	
	board_dbg("Si57x: xtal frequency = %d Hz\n", (int) f_xtal );

	if( freq_hz )
		*freq_hz = f_xtal;

}

int si57x_calc_frequency( uint32_t f_xtal, uint32_t freq_hz, uint64_t *rfreq_out, int* hsdiv_out, int* n1_out )
{
	const uint8_t hsdiv_values[] = { 4, 5, 6, 7, 9, 11, 0 };
	int hsdiv_idx, n1;
	const uint64_t f_dco_min = 4850000000;
	const uint64_t f_dco_max = 5670000000;

		for( hsdiv_idx = 0; hsdiv_values[hsdiv_idx] != 0; hsdiv_idx++ )
		{
			for( n1 = 1; n1 <= 255; n1++ )
			{
				if ( n1 && (n1 & 1) )
					continue;

			uint64_t hs_div = hsdiv_values[hsdiv_idx];

			uint64_t f_dco = (uint64_t) freq_hz * hs_div * n1;

			if( f_dco < f_dco_min || f_dco > f_dco_max )
				continue;

			uint64_t rfreq = f_dco * (1ULL<<28) / f_xtal;

			*rfreq_out = rfreq;
			*hsdiv_out = hsdiv_idx;
			*n1_out = n1;

			return 0;
		}
	}
	return -1;
}

void si57x_reset(struct wr_si57x_interface_device *dev )
{
	uint8_t r135 = 1;

	r135 = (1<<7);
	si57x_write( dev, 135, &r135, 1 );
	timer_delay_ms(10);
	r135 =  1;
	si57x_write( dev, 135, &r135, 1 );

}


int si57x_set_frequency( struct wr_si57x_interface_device *dev, uint32_t f_xtal, uint32_t freq_hz )
{
	uint8_t regs[16];
	uint64_t rfreq;
	int hsdiv;
	int n1;
	int i;
	
	if( si57x_calc_frequency ( f_xtal, freq_hz, &rfreq, &hsdiv, &n1 ) < 0 )
		return -1;

	dev->n1 = n1;
	dev->hsdiv = hsdiv;
	dev->rfreq = rfreq;

	regs[12] = (dev->rfreq & 0xff);
	regs[11] = ((dev->rfreq >> 8) & 0xff);
	regs[10] = ((dev->rfreq >> 16) & 0xff);
	regs[9] =  ((dev->rfreq >> 24) & 0xff);
	regs[8] = ((dev->rfreq >>32) & 0x3f) | (((dev->n1-1) & 0xff) << 6);
	regs[7] = (dev->hsdiv << 5) | ((dev->n1-1) >> 2);

	uint8_t r137, r135;

	timer_delay_ms(10);

	writel( (uint32_t) ( rfreq & 0xffffffffULL), dev->base_addr + SI570_REG_RFREQL );
	writel( (uint32_t) ( rfreq >> 32) | (((n1-1) & 0xff) << 6), dev->base_addr + SI570_REG_RFREQH );
	writel( SI570_CR_ENABLE | SI570_CR_CLK_DIV_W(100) | SI570_CR_I2C_ADDR_W ( ( dev->i2c_addr << 1 ) ) | SI570_CR_GAIN_W(10), dev->base_addr + SI570_REG_CR );

	si57x_read( dev, 135, &r135, 1 );
	si57x_read( dev, 137, &r137, 1 );
	r137 |= (1<<4); // freeze DCO
	si57x_write( dev, 137, &r137, 1);
	si57x_write( dev, 7, regs + 7, 6 );
	r137 &= ~(1<<4); // unfreeze DCO
	si57x_write( dev, 137, &r137, 1);
	r135 |= (1<<6); // assert NewFreq
	si57x_write( dev, 135, &r135, 1);

	return 0;
}

#define PCA9554_REG_IN 0
#define PCA9554_REG_OUT 1
#define PCA9554_REG_INVERT 2
#define PCA9554_REG_CONFIG 3



static uint8_t pca9554_read_reg( struct pca9554_gpio_device *dev, uint8_t reg )
{
	uint8_t rv;
	bb_i2c_start(dev->bus);
	bb_i2c_put_byte(dev->bus, dev->i2c_addr << 1);
	bb_i2c_put_byte(dev->bus,  reg );
	bb_i2c_repeat_start(dev->bus );
	bb_i2c_put_byte(dev->bus,  (dev->i2c_addr << 1) | 1);
	bb_i2c_get_byte(dev->bus, &rv, 1 );
	bb_i2c_stop(dev->bus);
	return rv;
}

static void pca9554_write_reg( struct pca9554_gpio_device *dev, uint8_t reg, uint8_t value )
{
	bb_i2c_start(dev->bus);
	bb_i2c_put_byte(dev->bus, dev->i2c_addr << 1);
	bb_i2c_put_byte(dev->bus,  reg );
	bb_i2c_put_byte(dev->bus,  value );
	bb_i2c_stop(dev->bus);
}

static void pca9554_gpio_out(const struct gpio_pin *pin, int value)
{
	struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;


	uint8_t oreg = pca9554_read_reg( dev, PCA9554_REG_OUT );

	if( value )
		oreg |= ( 1<< pin->pin );
	else
		oreg &= ~ ( 1<< pin->pin );


	pca9554_write_reg( dev, PCA9554_REG_OUT, oreg );
}



static void pca9554_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
	struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;


	uint8_t dreg = pca9554_read_reg( dev, PCA9554_REG_CONFIG );

	if( ! dir )
		dreg |= ( 1<< pin->pin );
	else
		dreg &= ~ ( 1<< pin->pin );


	pca9554_write_reg( dev, PCA9554_REG_CONFIG, dreg );
	
}


static int pca9554_gpio_in(const struct gpio_pin *pin)
{
	struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;

// fixme: implement
	return 0;
}


void pca9554_gpio_init( struct pca9554_gpio_device *dev, struct i2c_bus *bus, uint8_t i2c_addr )
{
	dev->bus = bus;
	dev->i2c_addr = i2c_addr;
	dev->gpio.priv = (void *) dev;
	dev->gpio.read_pin = pca9554_gpio_in;
	dev->gpio.set_dir = pca9554_gpio_set_dir;
	dev->gpio.set_out = pca9554_gpio_out;
}

void wr_si57x_interface_init( struct wr_si57x_interface_device *dev, void* base_addr, uint8_t i2c_addr )
{

	dev->base_addr = base_addr;
	dev->gpio_i2c.priv = (void *) dev;
	dev->gpio_i2c.read_pin = si57x_gpio_in;
	dev->gpio_i2c.set_dir = si57x_gpio_set_dir;
	dev->gpio_i2c.set_out = si57x_gpio_out;
	dev->i2c_addr = i2c_addr;
	dev->pin_scl.device = &dev->gpio_i2c;
	dev->pin_scl.pin = SI57X_PIN_SCL;
	dev->pin_sda.device = &dev->gpio_i2c;
	dev->pin_sda.pin = SI57X_PIN_SDA;
	bb_i2c_create( &dev->master, &dev->pin_scl, &dev->pin_sda );
}


static int calc_apr(int meas_min, int meas_max, int f_center )
{
	// apr_min is in PPM

	if( f_center < meas_min || f_center > meas_max )
		f_center = (meas_min + meas_max) / 2;

	int64_t delta_low =  meas_min - f_center;
	int64_t delta_hi = meas_max - f_center;
	uint64_t u_delta_low, u_delta_hi;
	int ppm_lo, ppm_hi;

	if(delta_low >= 0)
		return -1;
	if(delta_hi <= 0)
		return -1;

	/* __div64_32 divides 64 by 32; result is in the 64 argument. */
	u_delta_low = -delta_low * 1000000LL;
	__div64_32(&u_delta_low, f_center);
	ppm_lo = (int)u_delta_low;

	u_delta_hi = delta_hi * 1000000LL;
	__div64_32(&u_delta_hi, f_center);
	ppm_hi = (int)u_delta_hi;

	return ppm_lo < ppm_hi ? ppm_lo : ppm_hi;
}

static void check_vco_freq( int cm_channel, int cm_ref, void (*dac_setter)(int ))
{
	int f_min, f_max;

	wb_cm_configure( &board.clk_mon, cm_ref, 5, 1000000 );
	wb_cm_set_ref_frequency( &board.clk_mon, CPU_CLOCK );

	dac_setter( 0 );
	timer_delay_ms(1);
	wb_cm_restart( &board.clk_mon );
	while( ! (wb_cm_read( &board.clk_mon ) & ( 1<< cm_channel) ) );
	f_min = board.clk_mon.freqs[ cm_channel ];
	dac_setter( 65535 );
	timer_delay_ms( 1 );
	wb_cm_restart( &board.clk_mon );
	while( ! (wb_cm_read( &board.clk_mon ) & ( 1<< cm_channel) ) );
	f_max = board.clk_mon.freqs[ cm_channel ];

	dac_setter( 32768 );
	timer_delay(1);

	pp_printf("VCO ch %d:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", cm_channel, f_min, f_max, calc_apr(f_min, f_max, 62500000) );
}


void set_dmtd_dac( int value )
{
	spll_set_dac( -1, value );
}

void set_main_dac( int value )
{
	spll_set_dac( 0, value );
}

const struct gpio_pin pin_rtm_4sfp_led_orange = { &board.gpio_rtm_main.gpio, 3 };
const struct gpio_pin pin_rtm_4sfp_i2c_reset_n = { &board.gpio_rtm_main.gpio, 5 };


const struct gpio_pin pin_rtm_4sfp_sfp_tx_disable = { &board.gpio_rtm_sfp.gpio, 1 };

void sfp_setup()
{
	board_dbg("Check RTM & init SFPs...\n");
	//bb_i2c_scan( &board.si57x.master );
	tca9548_select_channels( &board.si57x.master, 0x70, 1 << AFCZ_I2C_MUX_CHANNEL_RTM );
	//bb_i2c_scan( &board.si57x.master );

	pca9554_gpio_init( &board.gpio_rtm_main, &board.si57x.master, 0x20 ); // fixme : constants
	pca9554_gpio_init( &board.gpio_rtm_sfp, &board.si57x.master, 0x22 ); // fixme : constants

	gen_gpio_out( &pin_rtm_4sfp_i2c_reset_n, 0 );
	gen_gpio_out( &pin_rtm_4sfp_i2c_reset_n, 1 );


	const int sfp_busses [] = 
	{
		RTM_4SFP_MUX_SFP0,
		RTM_4SFP_MUX_SFP1,
		RTM_4SFP_MUX_SFP2,
		RTM_4SFP_MUX_SFP3,
		RTM_4SFP_MUX_SFP4,
		RTM_4SFP_MUX_SFP5,
		RTM_4SFP_MUX_SFP6,
		-1
	};
	int i;

	for( i = 0; sfp_busses[i] >= 0; i++ )
	{
		// select SFPx
		tca9548_select_channels( &board.si57x.master, 0x74, 1 << sfp_busses[i] );

		gen_gpio_set_dir( &pin_rtm_4sfp_sfp_tx_disable, 1 );
		gen_gpio_out( &pin_rtm_4sfp_sfp_tx_disable, 0 );
	}



}


void afcz_read_persistent_mac()
{
	uint8_t mac_addr[6];

	tca9548_select_channels( &board.si57x.master, 0x70, 1 << AFCZ_I2C_MUX_CHANNEL_RTM );
	
	i2c_eeprom_create( &board.mac_eeprom, &board.si57x.master, AFCZ_I2C_ADDR_MAC_EEPROM, 1 );
	int n_read = i2c_eeprom_read( &board.mac_eeprom, AFCZ_I2C_EEPROM_MAC_OFFSET, mac_addr, 6);

	if( n_read != 6 )
	{
		board_dbg("Failed to get MAC address from MAC EEPROM. Using fallback address.\n");
		mac_addr[0] = 0x22;
		mac_addr[1] = 0x33;
		mac_addr[2] = 0x44;	/* fallback MAC if get_persistent_mac fails */
		mac_addr[3] = 0x55;
		mac_addr[4] = 0x66;
		mac_addr[5] = 0x77;
	}

    ep_set_mac_addr( mac_addr );

	board_dbg("Local MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
		mac_addr[4], mac_addr[5]);
}


int wrc_board_early_init()
{
//	wb_gpio_create( &board.gpio_aux, 0x48000 );
	board_dbg("WR Core AFCZ port starting up\n");    

	wr_si57x_interface_init( &board.si57x, BASE_SI57X_INTERFACE, SI57X_I2C_ADDR );
	tca9548_select_channels( &board.si57x.master, 0x70, 1 << AFCZ_I2C_MUX_CHANNEL_SI570 );

	net_rst();
	ep_init();
	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable(1, 1);
	timer_delay_ms(200);

	uint8_t regs[16];

	si57x_reset( &board.si57x );

	timer_delay_ms(10);

	si57x_read( &board.si57x, 0, regs, 16 ); 
	int i;

	int32_t f_xtal;


	si57x_get_xtal_frequency( &board.si57x, &f_xtal );
	si57x_set_frequency( &board.si57x, f_xtal, 125000000 );

	idt8v_clock_mux_init ( &board.clk_mux, &board.si57x.master, IDT8V_I2C_ADDR );
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_CLK_SI570_1_IN, 1, 1, 0);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_CLK_SI570_2_IN, 1, 1, 0);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_FPGA_CLK3_OUT, 0, 0, AFCZ_IC33_CLK_SI570_1_IN);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_FPGA_CLK_GTX_CUST2_OUT, 0, 0, AFCZ_IC33_CLK_SI570_1_IN);
	idt8v_commit_configuration ( &board.clk_mux );

	wb_cm_init( &board.clk_mon, BASE_CLOCK_MONITOR, 6 );

	sfp_setup();

	afcz_read_persistent_mac();

	tca9548_select_channels( &board.si57x.master, 0x70, 1 << AFCZ_I2C_MUX_CHANNEL_SI570 );

#if 1
	set_dmtd_dac(32767);
	set_main_dac(32767);

// cross-check the REF and DDMTD clocks
	pp_printf("Checking DDMTD and REF clock frequencies:\n");
	check_vco_freq( AFCZ_CM_CHANNEL_CLK_DMTD, AFCZ_CM_CHANNEL_CLK_REF, set_dmtd_dac );
	check_vco_freq( AFCZ_CM_CHANNEL_CLK_REF, AFCZ_CM_CHANNEL_CLK_DMTD, set_main_dac );

#endif
	return 0;
}

int wrc_board_init()
{
	/* initialize I2C bus */
	bb_i2c_init(&dev_i2c_fmc);

	/* init storage (we use the SPI flash on eRTM14) */
	bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, 10 );

	spi_wrc_flash.rd_falling_edge = 1;

	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, 0x10000, 0x1f00000 );

	storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
    storage_mount( &wrc_storage_dev );

    return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
