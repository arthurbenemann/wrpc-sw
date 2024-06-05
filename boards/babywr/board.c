/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2022 Nikhef (www.Nikhef.nl)
 * Author: Peter Jansweijer <peterj@nikhef.nl> based on work
 * from Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "board.h"
#include "wrc.h"
#include "lib/snmp.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "dev/pps_gen.h"
#include "storage.h"
#include "softpll_ng.h"
#include "hw/sit5359_regs.h"
#include <wrpc.h>

struct babywr_board board;

static spll_gain_schedule_t spll_main_ocxo_gain_sched;

#define SIT5359_PIN_SCL 0
#define SIT5359_PIN_SDA 1

// I2C_ADDR (A0=0) => 1100010 (WR:0xC4, RD:0xC5 or 0x62+r/w)
// I2C_ADDR (A0=1) => 1101010 (WR:0xD4, RD:0xD5 or 0x6A+r/w)
#define SIT5359_I2C_ADDR_A0_0 0x62
#define SIT5359_I2C_ADDR_A0_1 0x6A

#define DAC_HALF_SCALE (1<<(BOARD_SPLL_DAC_BITS - 1))
#define DAC_FULL_SCALE (1<<(BOARD_SPLL_DAC_BITS))

#if defined(CONFIG_SNMP) && defined(SNMP_SET)
/* Functions and variables used by the SNMP protocol to control the timing output */
static const uint8_t oid_wrpcSelGroup0[] =           {1,0};
static const uint8_t oid_wrpcSelGroup1[] =           {2,0};

/* oid_wprcBoardSpecific*/
const uint8_t oid_wrpcBoardSpecificGroup[] =    {0x2B,6,1,4,1,96,101,1,13};

/* wrpcBoardSpecificGroup array */
const struct snmp_oid oid_array_wrpcBoardSpecificGroup[] = {
	OID_FIELD_VAR(   oid_wrpcSelGroup0,  get_select_group, set_select_group, ASN_INTEGER,   &(board.gpio_main_board)),
	OID_FIELD_VAR(   oid_wrpcSelGroup1,  get_select_group, set_select_group, ASN_INTEGER,   &(board.gpio_main_board)),
	{ 0, }
};
static const int sel_group_offset = 3;

int set_select_group(uint8_t *buf, struct snmp_oid *obj){

	uint8_t io_stat;
	uint8_t len = buf[1];
	uint8_t *oid_data = buf + 2;
	uint8_t sel_group = *(buf - 2) + sel_group_offset;  // add offset to get the appropriate value from board.h
	uint8_t sel_group_reg = WBGEN2_GEN_MASK(sel_group, 1);
	uint8_t asn_incoming = buf[0];
	uint8_t asn_expected = obj->asn;
	uint32_t tmp_u32;

	if (asn_incoming != asn_expected) { /* wrong data type */
		snmp_verbose("%s: wrong asn 0x%02x, expected 0x%02x\n",
			     __func__, asn_incoming, asn_expected);
		return -SNMP_ERR_BADVALUE;
	}
	
	io_stat = pca9554_read_reg(obj->p, PCA9554_REG_OUT);

	memcpy(&tmp_u32, oid_data, len);
	tmp_u32 = ntohl(tmp_u32);
	/* move data when shorter than 4 bytes */
	tmp_u32 = tmp_u32 >> ((4 - len) * 8);

	if(tmp_u32){		
		pca9554_write_reg(obj->p, PCA9554_REG_OUT, io_stat | sel_group_reg);
	}
	else{
		pca9554_write_reg(obj->p, PCA9554_REG_OUT, io_stat & ~sel_group_reg);
	}

	return len + 2;
}

int get_select_group(uint8_t *buf, struct snmp_oid *obj){
	uint8_t *oid_data = buf + 2;
	uint8_t *len = &buf[1];
	uint32_t on = htonl(1);
	uint32_t off = htonl(0);
	uint8_t sel_group = *(buf - 2) + sel_group_offset; // add offset to get the appropriate value from board.h
	uint8_t reg_stat = pca9554_read_reg(obj->p, PCA9554_REG_OUT);
	uint8_t sel_group_reg = WBGEN2_GEN_MASK(sel_group, 1);
	uint8_t sel_group_status = reg_stat & sel_group_reg;

	*len = sizeof(uint32_t);
	buf[0] = obj->asn;
	if(sel_group_status){
		memcpy((char*)oid_data, &on, *len);
	}
	else{
		memcpy((char*)oid_data, &off, *len);
	}

	return *len + 2;
}

#endif


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
    int attempts = 0;

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
    bb_i2c_init(&dev->master);

    // Avoid failing initialization due to any pending I2C transfer from the past.
    // Probe the SiTime oscillators to align their I2C state machine.
    // Wait for a proper answer...
    while (!bb_i2c_devprobe(&dev->master, i2c_addr)) {
        attempts++;
        if (attempts > 10) {
            board_dbg("SiT5359 device not found.\n");
            break;
        }
    }
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
        bb_i2c_put_byte( &dev->master, data[i] );

    bb_i2c_stop( &dev->master );
}

static int sit5359_dev_init( struct wr_sit5359_interface_device *dev )
{
    // Enable SPLL and Osc Output Enable (PartNo option "I": hardware OE via pin 1)
    // I2C bus freqency = 1/(4*(30+1)*16ns) = 504 KHz
    writel( SIT5359_CR_SPLL_EN | SIT5359_CR_OSC_OE | SIT5359_CR_CLK_DIV_W(30) | SIT5359_CR_I2C_ADDR_W ( ( dev->i2c_addr << 1 ) ), dev->base_addr + SIT5359_REG_CR );

    return 0;
}

int conv_twos_compl (int val) {
    if (val < DAC_HALF_SCALE)
        return (DAC_FULL_SCALE - (~(val - DAC_HALF_SCALE) + 1));
    else
        return(val - DAC_HALF_SCALE);
}

int regs2dac (uint8_t *regs)
{
    int dac_twos = (((regs[1] & 0xff) << (SIT5359_DFC_BITS-26)) | // SiT5359 Reg 0x00 7:0  => DFC-LSW[7:0]
                    ((regs[0] & 0xff) << (SIT5359_DFC_BITS-18)) | // SiT5359 Reg 0x00 15:8 => DFC-LSW[15:8]
                    ((regs[3] & 0xff) << (SIT5359_DFC_BITS-10)) | // SiT5359 Reg 0x01 7:0  => DFC-MSW[7:0]
                    ((regs[2] & 0x03) << (SIT5359_DFC_BITS-2 ))); // SiT5359 Reg 0x01 15:8 => DFC-MSW[9:8]
    dac_twos = dac_twos >> (SIT5359_DFC_BITS - BOARD_SPLL_DAC_BITS);
    return conv_twos_compl(dac_twos);
}

void dac2regs (uint32_t dac, uint8_t * regs)
{
    int dac_twos = conv_twos_compl(dac);
    const char SITIME_OE = 0x04;            // bit 2 of register regs[2] = bit 10 of SiTime address 0x01

    dac_twos = dac_twos << (SIT5359_DFC_BITS - BOARD_SPLL_DAC_BITS);
    regs[1] =  (dac_twos >> (SIT5359_DFC_BITS-26)) & 0xff; // SiT5359 Reg 0x00 7:0  => DFC-LSW[7:0]
    regs[0] =  (dac_twos >> (SIT5359_DFC_BITS-18)) & 0xff; // SiT5359 Reg 0x00 15:8 => DFC-LSW[15:8]
    regs[3] =  (dac_twos >> (SIT5359_DFC_BITS-10)) & 0xff; // SiT5359 Reg 0x01 7:0  => DFC-MSW[7:0]
    regs[2] = ((dac_twos >> (SIT5359_DFC_BITS-2 )) & 0x3)  // SiT5359 Reg 0x01 15:8 => DFC-MSW[9:8]
              | SITIME_OE;                                 // (PartNo option "I": hardware OE via pin 1)
    regs[5] = 0x01;                                        // SiT5359 Reg 0x02 7:0  => Pull Range 10 ppm
    regs[4] = 0x00;                                        // SiT5359 Reg 0x02 15:8 => not used
}

void read_sitime (void)
{
    uint8_t regs[6];

    // read back all SiT5359_refclk registers (6 bytes)
    sit5359_read(&board.sit5359_refclk, 0x00, regs, 6 );
    board_dbg("SiT5359 RefClk Regs: %02x:%02x:%02x:%02x:%02x:%02x DAC: %d\n",regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs2dac(regs));
    // read back all SiT5359_dmtd registers (6 bytes)
    sit5359_read(&board.sit5359_dmtd, 0x00, regs, 6 );
    board_dbg("SiT5359 DMTD Regs:   %02x:%02x:%02x:%02x:%02x:%02x DAC: %d\n",regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs2dac(regs));
}

void write_sitime (int dev, int val)
{
    uint8_t regs[6];

    dac2regs(val, regs);

    if (dev == 0)
        sit5359_i2c_write(&board.sit5359_refclk, 0x00, regs, 6 );
    else
        sit5359_i2c_write(&board.sit5359_dmtd, 0x00, regs, 6 );
}

// ======================================
// GPIO Control functions
// ======================================

int gpio_control_poll()
{
    static int prev_servo_state = 0;
    static int prev_link_state = 0;
    int link_state = 0;

    uint8_t io_stat;
    uint64_t sec;
    uint32_t nsec;

    extern struct pp_instance ppi_static;
    struct pp_instance *ppi = &ppi_static;
//    struct wr_servo_state *s =
//			&((struct wr_data *)ppi->ext_data)->servo_state;
    int curr_servo_state = ppi->servo->state;

    if (prev_servo_state != WRH_TRACK_PHASE && curr_servo_state == WRH_TRACK_PHASE) {
        shw_pps_gen_get_time(&sec, &nsec);
        board_dbg("TRACK_PHASE: '%s'\n",format_time(sec, TIME_FORMAT_LEGACY));
        io_stat = pca9554_read_reg(&board.gpio_main_board, PCA9554_REG_IN);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat | MAIN_BOARD_LED_0);
    }
    if (prev_servo_state == WRH_TRACK_PHASE && curr_servo_state != WRH_TRACK_PHASE) {
        shw_pps_gen_get_time(&sec, &nsec);
        board_dbg("LOST TRACK_PHASE: '%s'\n",format_time(sec, TIME_FORMAT_LEGACY));
        io_stat = pca9554_read_reg(&board.gpio_main_board, PCA9554_REG_IN);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat & ~MAIN_BOARD_LED_0);
    }

    link_state = ep_link_up( &wrc_endpoint_dev, NULL);
    if (!prev_link_state && link_state) {
        shw_pps_gen_get_time(&sec, &nsec);
        board_dbg("Link up: '%s'\n",format_time(sec, TIME_FORMAT_LEGACY));
        io_stat = pca9554_read_reg(&board.gpio_main_board, PCA9554_REG_IN);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat | MAIN_BOARD_LED_1);
	} else if (prev_link_state && !link_state) {
        shw_pps_gen_get_time(&sec, &nsec);
        board_dbg("Link down: '%s'\n",format_time(sec, TIME_FORMAT_LEGACY));
        io_stat = pca9554_read_reg(&board.gpio_main_board, PCA9554_REG_IN);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat & ~MAIN_BOARD_LED_1);
	}

    prev_servo_state = curr_servo_state;
    prev_link_state = link_state;

    return 0;
}

void gpio_control_init()
{
    int i;
    uint8_t io_stat;
    board_dbg("Initializing GPIO control...\n");
    pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_CONFIG, 0x00);  // Configure all IO as output
    pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, MAIN_BOARD_FAN_ENABLE);     // LEDs, SEL_GROUP_0/1 and SEL_IRIG_B all '0', FAN_ENABLE = '1'

    for( i = 0 ; i < 10; i++ )
        {
        io_stat = pca9554_read_reg(&board.gpio_main_board, PCA9554_REG_OUT);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat | MAIN_BOARD_LED_3);
        timer_delay_ms(300);
        pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, io_stat & ~MAIN_BOARD_LED_3);
        timer_delay_ms(300);
    }
}

static void babywr_spll_setup(void)
{

int implement_two_stages = 0; // implement 2-stage ocxo lock later

/* configure a suitable PI gain schedule for the SoftPLL: */
    spll_gain_schedule_t* gs=  &spll_main_ocxo_gain_sched;

/* we start with the default SiT5359 values (Bandwidth ~20 Hz) */
    gs->stages[0].kp = -5000;  // use 1400 when X1 = 125 MHz
    gs->stages[0].ki = -30;
    gs->stages[0].lock_samples = 10000;
    gs->stages[0].shift = 12;

/* once it's locked, the loop bandwidth is switched to low bandwidth  to filter out WR link added phase noise */
    gs->stages[1].kp = -600;
    gs->stages[1].ki = -2;
    gs->stages[1].lock_samples = 10000;
    gs->stages[1].shift = 12;

    if ( implement_two_stages ) {
        gs->n_stages = 2;   // 2 stages: OCXO
        board_dbg("Oscillator gain schedule: Two stage OCXO setup\n");
        spll_set_gain_schedule( gs );
    } else {
        gs->n_stages = 1;   // 1 stage: SiTime 5359
        board_dbg("Oscillator gain schedule: 1st stage SiTime setup\n");
        spll_set_gain_schedule( gs );
    }
}

static struct gpio_pin pin_eeprom_scl        = { &board.gpio_aux, 0 };
static struct gpio_pin pin_eeprom_sda        = { &board.gpio_aux, 1 };
static struct gpio_pin pin_aux_scl           = { &board.gpio_aux, 2 };
static struct gpio_pin pin_aux_sda           = { &board.gpio_aux, 3 };
static struct gpio_pin pin_spare0            = { &board.gpio_aux, 4 };
static struct gpio_pin pin_spare1            = { &board.gpio_aux, 5 };


struct i2c_bus            i2c_wrc_eeprom;
struct i2c_bus            dev_i2c_aux;
struct i2c_eeprom_device  wrc_eeprom_dev;
struct i2c_eeprom_device  wrc_uid_dev;

int wrc_board_early_init()
{
    /* most of the I/Os of the slow peripherals (i2c, spi) are bitbanged. First, let's
       initialize the GPIO controller they're connected to */
    wb_gpio_create( &board.gpio_aux, BASE_GPIO );
    
    /* create and initialize eeprom I2C bus */
    bb_i2c_create(&i2c_wrc_eeprom,
         &pin_eeprom_scl,
         &pin_eeprom_sda );
    bb_i2c_init(&i2c_wrc_eeprom);

    /* create and initialize auxiliary I2C bus */
    bb_i2c_create(&dev_i2c_aux,
         &pin_aux_scl,
         &pin_aux_sda );
    bb_i2c_init(&dev_i2c_aux);

    i2c_eeprom_create(&wrc_eeprom_dev, &i2c_wrc_eeprom, FMC_EEPROM_ADR, 2);
    storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_dev );

    /*
     * Mount SDBFS filesystem from storage.
     */
    storage_mount( &wrc_storage_dev );
    
    /* create and initialize UID eeprom I2C bus */
    i2c_eeprom_create(&wrc_uid_dev, &i2c_wrc_eeprom, UID_EEPROM_ADR, 1);

    wr_sit5359_interface_init( &board.sit5359_refclk, BASE_SIT5359_REFCLK, SIT5359_I2C_ADDR_A0_1 );
    wr_sit5359_interface_init( &board.sit5359_dmtd, BASE_SIT5359_DMTD, SIT5359_I2C_ADDR_A0_0 );

    /* Initialize I2C bus multiplexer */
    pca9554_gpio_init( &board.gpio_main_board, &dev_i2c_aux, PCA9554_ADR );

    /* Setup the SoftPLL for the OCXO we have */
    babywr_spll_setup();

    return 0;
}

int wrc_board_init()
{
    uint8_t regs[6];
    int i;

    // set I2C bus speed and OSC Output enable
    sit5359_dev_init(&board.sit5359_refclk);
    sit5359_dev_init(&board.sit5359_dmtd);

    // initialize registers
    dac2regs(DAC_HALF_SCALE, regs);
    sit5359_i2c_write(&board.sit5359_refclk, 0x00, regs, 6 );
    sit5359_i2c_write(&board.sit5359_dmtd, 0x00, regs, 6 );

    // read back the SiT5359 oscillators
    read_sitime();

    uint8_t mac_addr[6];
    /*
     * Read MAC addr from Unique-ID, IC D12, 24AA025E48
     */

    i2c_eeprom_read(&wrc_uid_dev, UID_OFFSET , mac_addr, sizeof(mac_addr));
    board_dbg("MAC addr: %x:%x:%x:%x:%x:%x\n",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

    ep_set_mac_addr(&wrc_endpoint_dev, mac_addr);
    ep_pfilter_init_default(&wrc_endpoint_dev);

    pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_CONFIG, 0x00);  // Configure all IO as output
    pca9554_write_reg(&board.gpio_main_board, PCA9554_REG_OUT, 0x00);     // LEDs, SEL_GROUP_0/1 and SEL_IRIG_B all '0'

    for( i = 0 ; i < 50; i++ )
        {
        gen_gpio_out( &pin_spare0, 0 );
        gen_gpio_out( &pin_spare1, 1 );
        timer_delay_ms(300);
        gen_gpio_out( &pin_spare0, 1 );
        gen_gpio_out( &pin_spare1, 0 );
        timer_delay_ms(300);
    }

    return 0;
}

int wrc_board_create_tasks()
{
   wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );
   wrc_task_create( "pgpio_control", gpio_control_init, gpio_control_poll );
   return 0;
}
