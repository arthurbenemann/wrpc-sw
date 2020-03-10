#include "board.h"

#include "dev/syscon.h"
#include "dev/flash.h"
#include "dev/i2c.h"
#include "dev/onewire.h"
#include "dev/w1.h"
#include "dev/gpio.h"
#include "dev/bb_i2c.h"
#include "dev/clock_monitor.h"

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

struct {
	struct gpio_device gpio_aux;
	struct wr_si57x_interface_device si57x;
	struct wb_clock_monitor_device clk_mon;
	struct idt8v_clock_mux_device clk_mux;
} board;

static uint8_t idt8v_read_regs( struct idt8v_clock_mux_device*dev )
{
	int i;
	bb_i2c_start( dev->bus );
	bb_i2c_put_byte( dev->bus, (dev->i2c_addr << 1) | 1 );
	for( i = 0; i < 16; i++ )
		bb_i2c_get_byte( dev->bus, &dev->regs[i], i == 15 ? 1 : 0 );
	bb_i2c_stop( dev->bus );

	pp_printf("Idt8v regs:\n");
	for(i=0;i<16;i++)
		pp_printf("r%d = %x\n", i, dev->regs[i]);
}

static uint8_t idt8v_commit_configuration( struct idt8v_clock_mux_device*dev )
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

	pp_printf("cfg r %d = %x\n", io_index, r );
	dev->regs[ io_index ] = r;
}



static void si57x_gpio_out(const struct gpio_pin *pin, int value)
{
	struct wr_si57x_interface_device* dev = ( struct wr_si57x_interface_device* ) pin->device->priv;

	

	uint32_t mask = (pin->pin == SI57X_PIN_SCL ? SI570_GPCR_SCL : SI570_GPCR_SDA );
	uint32_t reg = (value ? SI570_REG_GPSR : SI570_REG_GPCR );

//	pp_printf("gpio: base %p pin %d mask %d reg %x value %x\n", dev->base_addr, pin->pin, mask, reg, value );


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

//	pp_printf("RFREQ %08x %08x n1 %d hsdiv %d\n", (uint32_t) (rfreq >> 32), (uint32_t) rfreq, (int)n1, (int)hs_div );

	uint64_t f0 = 100000000;
	uint64_t f_xtal = (f0 * hs_div * n1 ) * ( 1ULL << 28 ) / rfreq;

	if( freq_hz )
		*freq_hz = f_xtal;

    pp_printf("f_xtal %d\n",  (uint32_t) f_xtal );

	
}

int si57x_calc_frequency( uint32_t f_xtal, uint32_t freq_hz, uint8_t *regs )
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

			pp_printf("CalcF %d RFREQ %08x %08x n1 %d hsdiv %d\n", freq_hz, (uint32_t) (rfreq >> 32), (uint32_t) rfreq, (int)n1, (int)hs_div );

			regs[12] = (rfreq & 0xff);
			regs[11] = ((rfreq >> 8) & 0xff);
			regs[10] = ((rfreq >> 16) & 0xff);
			regs[9] =  ((rfreq >> 24) & 0xff);
			regs[8] = ((rfreq >>32) & 0x3f) | (((n1-1) & 0xff) << 6);
			regs[7] = (hsdiv_idx << 5) | ((n1-1) >> 2);
			
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

	int i;
	
	if( si57x_calc_frequency ( f_xtal, freq_hz, regs ) < 0 )
		return -1;

	uint8_t r137, r135;

	pp_printf("WR: ");
	for(i=0;i<16;i++)
		pp_printf("%02x ", regs[i]);
	pp_printf("\n");

	si57x_read( dev, 135, &r135, 1 );
	si57x_read( dev, 137, &r137, 1 );
	r137 |= (1<<4); // freeze DCO
	pp_printf("r135 %x r137 %x\n", r135, r137 );
	si57x_write( dev, 137, &r137, 1);

	si57x_write( dev, 7, regs + 7, 6 );

	r137 &= ~(1<<4); // unfreeze DCO
	si57x_write( dev, 137, &r137, 1);

	r135 |= (1<<6); // assert NewFreq
	si57x_write( dev, 135, &r135, 1);
	return 0;
}


void wr_si57x_interface_init( struct wr_si57x_interface_device *dev, void* base_addr, uint8_t i2c_addr )
{

	pp_printf("Si57x IF @ %p\n", base_addr );
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

int wrc_board_early_init()
{
//	wb_gpio_create( &board.gpio_aux, 0x48000 );

	wr_si57x_interface_init( &board.si57x, BASE_SI57X_INTERFACE, SI57X_I2C_ADDR );
	tca9548_select_channels( &board.si57x.master, 0x70, 1 << 2 );

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

	pp_printf("Si57x readback: ");
	for(i = 0; i < 16; i++)
		pp_printf("%02x ", regs[i] );
	pp_printf("\n");

	uint32_t f_xtal;


	si57x_get_xtal_frequency( &board.si57x, &f_xtal );
	si57x_set_frequency( &board.si57x, f_xtal, 125000000 );

	
	pp_printf("Readback\n");
	si57x_read( &board.si57x, 0, regs, 16 ); 
	pp_printf("Si57x readback (post-program): ");

	for(i = 0; i < 16; i++)
		pp_printf("%02x ", regs[i] );
	pp_printf("\n");


	idt8v_clock_mux_init ( &board.clk_mux, &board.si57x.master, IDT8V_I2C_ADDR );
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_CLK_SI570_1_IN, 1, 0, 0);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_CLK_SI570_2_IN, 1, 0, 0);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_FPGA_CLK3_OUT, 0, 0, AFCZ_IC33_CLK_SI570_1_IN);
	idt8v_configure_io ( &board.clk_mux, AFCZ_IC33_FPGA_CLK_GTX_CUST2_OUT, 0, 0, AFCZ_IC33_CLK_SI570_1_IN);
	idt8v_commit_configuration ( &board.clk_mux );

for(;;);
	idt8v_read_regs( &board.clk_mux );

	timer_delay_ms(2000);

	wb_cm_init( &board.clk_mon, BASE_CLOCK_MONITOR, 6 );
	wb_cm_configure( &board.clk_mon, AFCZ_CM_CHANNEL_CLK_SYS, 8, 625000 );
	wb_cm_set_ref_frequency( &board.clk_mon, CPU_CLOCK );

	for( i = 0; i < 5; i++)
	{

		wb_cm_restart( &board.clk_mon );
		timer_delay_ms(2000);
		wb_cm_read( &board.clk_mon );
		int j;
		for(j = 0; j < 6; j++)
		pp_printf("f%d = %d Hz [%d]\n", j, board.clk_mon.freqs[j], board.clk_mon.freq_valid_mask & (1<<j) ? 1 : 0);

	}


	return 0;
}

int wrc_board_init()
    {
	pp_printf("WR Core AFCZ port starting up\n");    
/*initialize flash*/
	flash_init();
	/*initialize I2C bus*/
	mi2c_init(WRPC_FMC_I2C);
	/*init storage (Flash / W1 EEPROM / I2C EEPROM*/
	storage_init(WRPC_FMC_I2C, FMC_EEPROM_ADR);

        wrpc_w1_init();
	wrpc_w1_bus.detail = ONEWIRE_PORT;
	w1_scan_bus(&wrpc_w1_bus);

	uint8_t mac_addr[6];


	if (get_persistent_mac(ONEWIRE_PORT, mac_addr) == -1) {
		pp_printf("Unable to determine MAC address\n");
		mac_addr[0] = 0x22;
		mac_addr[1] = 0x33;
		mac_addr[2] = 0x44;	/* fallback MAC if get_persistent_mac fails */
		mac_addr[3] = 0x55;
		mac_addr[4] = 0x66;
		mac_addr[5] = 0x77;
	}

        ep_set_mac_addr( mac_addr );

	pp_printf("Local MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
		mac_addr[4], mac_addr[5]);

        return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
