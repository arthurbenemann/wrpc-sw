/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
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


#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"
#include "dev/ad951x.h"
#include "dev/ltc6950.h"
#include "dev/ad9910.h"
#include "dev/ad9520.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/ertm15_rf_distr.h"
#include "dev/spi_flash.h"
#include "dev/i2c.h"
#include "softpll_ng.h"
#include "endpoint.h"

struct ertm14_board board;

static struct gpio_pin pin_pll_main_cs_n = { &board.gpio_aux, 0 };
static struct gpio_pin pin_pll_main_sdi = { &board.gpio_aux, 1 };
static struct gpio_pin pin_pll_main_sdo = { &board.gpio_aux, 2 };
static struct gpio_pin pin_pll_main_sclk = { &board.gpio_aux, 3 };
static struct gpio_pin pin_pll_main_reset = { &board.gpio_aux, 4 };
static struct gpio_pin pin_pll_main_lock = { &board.gpio_aux, 5 };

static struct gpio_pin pin_pll_ext_cs_n = { &board.gpio_aux, 6 };
static struct gpio_pin pin_pll_ext_sdi = { &board.gpio_aux, 7 };
static struct gpio_pin pin_pll_ext_sdo = { &board.gpio_aux, 8 };
static struct gpio_pin pin_pll_ext_sclk = { &board.gpio_aux, 9 };
static struct gpio_pin pin_pll_ext_reset = { &board.gpio_aux, 10 };
static struct gpio_pin pin_pll_ext_lock = { &board.gpio_aux, 11 };

static struct gpio_pin pin_mac_addr_scl = { &board.gpio_aux, 13 };
static struct gpio_pin pin_mac_addr_sda = { &board.gpio_aux, 12 };

static struct gpio_pin pin_main_xo_en_n = { &board.gpio_aux, 14 };

static struct gpio_pin pin_ltc6950_sclk = { &board.gpio_aux, 15 };
static struct gpio_pin pin_ltc6950_sdi = { &board.gpio_aux, 16 };
static struct gpio_pin pin_ltc6950_sdo = { &board.gpio_aux, 17 };
static struct gpio_pin pin_ltc6950_ce_gen = { &board.gpio_aux, 18 };
static struct gpio_pin pin_ltc6950_ce_distr = { &board.gpio_aux, 19 };
static struct gpio_pin pin_ltc6950_sync = { &board.gpio_aux, 20 };

static struct gpio_pin pin_ad9910_lo_sdio = { &board.gpio_aux, 4+21 };
static struct gpio_pin pin_ad9910_lo_sclk = { &board.gpio_aux, 5+21 };
static struct gpio_pin pin_ad9910_lo_reset = { &board.gpio_aux, 6+21 };
static struct gpio_pin pin_ad9910_lo_io_update = { &board.gpio_aux, 3+21 };

static struct gpio_pin pin_ad9910_ref_sdio = { &board.gpio_aux, 4+28 };
static struct gpio_pin pin_ad9910_ref_sclk = { &board.gpio_aux, 5+28 };
static struct gpio_pin pin_ad9910_ref_reset = { &board.gpio_aux, 6+28 };
static struct gpio_pin pin_ad9910_ref_io_update = { &board.gpio_aux, 3+28 };
const struct gpio_pin pin_ad9910_ref_sync_smp_err = { &board.gpio_aux, 5+28 };

static struct gpio_pin pin_ocxo_override = { &board.gpio_aux, 48 };
static struct gpio_pin pin_ocxo_cs_n = { &board.gpio_aux, 51 };
static struct gpio_pin pin_ocxo_sclk = { &board.gpio_aux, 50 };
static struct gpio_pin pin_ocxo_data = { &board.gpio_aux, 49 };

static struct gpio_pin pin_pwrmon_adc_cs_n = {  &board.gpio_aux, 52 };
static struct gpio_pin pin_pwrmon_adc_dout = {  &board.gpio_aux, 46 };
static struct gpio_pin pin_pwrmon_adc_din = {  &board.gpio_aux, 47 };
static struct gpio_pin pin_pwrmon_adc_sclk = {  &board.gpio_aux, 45 };

static struct gpio_pin pin_flash_cs_n = {  &board.gpio_aux, 55 };
static struct gpio_pin pin_flash_miso = {  &board.gpio_aux, 53 };
static struct gpio_pin pin_flash_mosi = {  &board.gpio_aux, 54 };
static struct gpio_pin pin_flash_sck = {  &board.gpio_aux, 56 };

static struct gpio_pin pin_ad9520_clka_scl = {  &board.gpio_aux, 57 };
static struct gpio_pin pin_ad9520_clka_sda = {  &board.gpio_aux, 58 };
static struct gpio_pin pin_ad9520_clkb_scl = {  &board.gpio_aux, 59 };
static struct gpio_pin pin_ad9520_clkb_sda = {  &board.gpio_aux, 60 };


#if 0
struct spi_bus spi_pll_main;
struct spi_bus spi_pll_ext;
struct spi_bus spi_ltc6950;
struct spi_bus spi_ad9910_ref;
struct spi_bus spi_ad9910_lo;
struct spi_bus spi_ocxo_dac;
struct spi_bus spi_ad7888;
struct spi_bus spi_flash;
struct i2c_bus i2c_clka_distr;
struct i2c_bus i2c_clkb_distr;

struct ad951x_device ad9516_main;
struct ad951x_device ad9516_ext;
struct ltc6950_device ltc6950_pll;
struct ad9910_device dds_ad9910_ref;
struct ad9910_device dds_ad9910_lo;
struct ad7888_device pwrmon_adc;
struct ertm15_rf_distribution_device rf_distr;
struct spi_flash_device dev_flash;
struct ad9520_device dev_clka_distr;
struct ad9520_device dev_clkb_distr;

struct i2c_bus i2c_mac_addr;

struct m24aa025_device m24_mac_ids[2];
#endif


static struct ad951x_config pll_main_dot050_config =
#include "ertm_14_pll_main_dot050_config.h"

static struct ad951x_config pll_main_ocxo_config =
#include "ertm_14_pll_ocxo_config.h"

static struct ltc6950_config pll_ertm15_config =
#include "ertm_15_ltc6950_config.h"

static spll_gain_schedule_t spll_main_ocxo_gain_sched;

static void ertm14_spll_setup(void)
{
/* configure a suitable PI gain schedule for the SoftPLL: */
    spll_gain_schedule_t* gs=  &spll_main_ocxo_gain_sched;

    gs->n_stages = 2;

/* we start with ~100 Hz bandwidth to make it lock reasonably fast */
    gs->stages[0].kp = -1100;
    gs->stages[0].ki = -30;
    gs->stages[0].lock_samples = 10000;
    gs->stages[0].shift = PI_FRACBITS;

/* once it's locked, the loop bandwidth is switched to ~0.1 Hz to filter out WR link added phase noise */
    gs->stages[1].kp = -5000;
    gs->stages[1].ki = -8;
    gs->stages[1].lock_samples = 10000;
    gs->stages[1].shift = PI_FRACBITS;
    
	spll_set_gain_schedule( gs );
}


static void ad9910_set_fine_delay( struct dds_sync_unit_channel *ch, int n_taps )
{
    pp_printf("SetFD ch %d taps %d\n", ch->index, n_taps );
}

static void ertm14_dds_trigger_ioupdate( struct ad9910_device *dev )
{
    int channel = (dev == &board.dds_ad9910_ref ? ERTM14_DDS_IOUPDATE_REF : ERTM14_DDS_IOUPDATE_LO);
    
    pp_printf("TrigIOUpdate channel %d\n", channel );
    dds_sync_force_pulse( &board.dds_sync_dev, channel );
}

static int ertm14_dds_sync_init()
{
    static int ch_delays[] = { 100000, 100000, 100000, 100000, 1000000, 1000000 };

    dds_sync_unit_create( &board.dds_sync_dev, BASE_ERTM14_DDS_SYNC_UNIT );

    dds_sync_unit_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 1, ch_delays[0], 0, 0 );

    dds_sync_unit_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_LO, 1, ch_delays[ERTM14_DDS_IOUPDATE_LO], 0, 0 );
    dds_sync_unit_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_REF, 1, ch_delays[ERTM14_DDS_IOUPDATE_REF], 0, 0 );


/*    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_DDS_LO, 1, ch_delays[1], 0 );
    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_CLKA, 1, ch_delays[2], 1 );
    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_CLKB, 1, ch_delays[3], 1 );*/

    dds_sync_unit_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 75, ad9910_set_fine_delay );
    dds_sync_unit_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 75, ad9910_set_fine_delay );
    return 0;
}

void ertm14_init(void)
{
    int i;
    uint32_t id;

    wb_gpio_create( &board.gpio_aux, BASE_AUXWB );

    gen_gpio_set_dir(&pin_main_xo_en_n, 1);
    gen_gpio_out(&pin_main_xo_en_n, 0);


    bb_spi_create ( &board.spi_pll_main,
        &pin_pll_main_cs_n,
        &pin_pll_main_sdi,
        &pin_pll_main_sdo,
        &pin_pll_main_sclk,
        AD951X_BIT_DELAY
        );

    
    bb_spi_create ( &board.spi_pll_ext,
        &pin_pll_ext_cs_n,
        &pin_pll_ext_sdi,
        &pin_pll_ext_sdo,
        &pin_pll_ext_sclk,
        AD951X_BIT_DELAY
        );

    
    bb_spi_create( &board.spi_ltc6950,
        &pin_ltc6950_ce_gen,
        &pin_ltc6950_sdi,
        &pin_ltc6950_sdo,
        &pin_ltc6950_sclk,
        100 );

    
   bb_spi_create( &board.spi_ad9910_ref,
        NULL,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sclk,
        100 );


    bb_spi_create( &board.spi_ad9910_lo,
        NULL,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sclk,
        100 );


    ltc6950_init(&board.ltc6950_pll, &board.spi_ltc6950);

    id = ltc6950_read( &board.ltc6950_pll, 0x16 );
    
    if( id != 0x65 )
    {
        pp_printf("Error initializing LTC6950 (read RevID: 0x%x, expected: 0x%x)\n", id, 0x65 );
    }

    ad951x_init(&board.ad9516_main, &board.spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock );
    ad951x_init(&board.ad9516_ext, &board.spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock );

    //ad951x_configure(&ad9516_main, &pll_main_dot050_config);
    ad951x_configure(&board.ad9516_main, &pll_main_ocxo_config);

    ltc6950_configure(&board.ltc6950_pll, &pll_ertm15_config);

    wb_cm_init(&board.ertm14_cmon, BASE_CLOCK_MONITOR, 5);
    wb_cm_configure(&board.ertm14_cmon, 0, 2, 6250000 );
    wb_cm_restart(&board.ertm14_cmon);

    gen_gpio_out(&pin_ocxo_override, 0);

    bb_spi_create( &board.spi_ocxo_dac,
        &pin_ocxo_cs_n,
        &pin_ocxo_data,
        &pin_ocxo_data,
        &pin_ocxo_sclk,
        100 );

    gen_gpio_out(&pin_ad9910_ref_reset, 1);
    gen_gpio_out(&pin_ad9910_lo_reset, 1);

    usleep(10);

    gen_gpio_out(&pin_ad9910_ref_reset, 0);
    gen_gpio_out(&pin_ad9910_lo_reset, 0);

    ertm14_dds_sync_init();

//    board.dds_ad9910_ref.pin_ioupdate = &pin_ad9910_ref_io_update;
//    board.dds_ad9910_lo.pin_ioupdate = &pin_ad9910_lo_io_update;

    ad9910_probe( &board.dds_ad9910_ref, &board.spi_ad9910_ref, ertm14_dds_trigger_ioupdate );
    ad9910_probe( &board.dds_ad9910_lo, &board.spi_ad9910_lo, ertm14_dds_trigger_ioupdate );

/* Unique MAC address storage chips (eRTM14 - IC7 and IC8) */

    bb_i2c_init( &board.i2c_mac_addr, &pin_mac_addr_scl, &pin_mac_addr_sda );
    m24aa025_init( &board.m24_mac_ids[0], &board.i2c_mac_addr, 0x50 );
    m24aa025_init( &board.m24_mac_ids[1], &board.i2c_mac_addr, 0x51 );

    uint8_t mac[6];

    m24aa025_read_mac( &board.m24_mac_ids[0], mac );
    ep_set_mac_addr( mac );

/* RF Power Monitor ADC (eRTM15 - IC43) */
    bb_spi_create( &board.spi_ad7888,
        &pin_pwrmon_adc_cs_n,
        &pin_pwrmon_adc_din,
        &pin_pwrmon_adc_dout,
        &pin_pwrmon_adc_sclk,
        100 );

    ad7888_create( &board.pwrmon_adc, &board.spi_ad7888 );

/* RF distribution switches and shift registers controlling these (eRTM15 - IC26..28) */
    ertm15_rf_distr_init( &board.rf_distr, &board.pwrmon_adc );

    ad9910_program(&board.dds_ad9910_ref, 205000000ULL, 0, 0x0 );
    ad9910_program(&board.dds_ad9910_lo, 209000000ULL, 0, 0x0 );

    ertm14_spll_setup();

    gen_gpio_set_dir( &pin_flash_mosi, 1 );
    gen_gpio_set_dir( &pin_flash_cs_n, 1 );
    gen_gpio_set_dir( &pin_flash_sck, 1 );

    bb_spi_create( &board.spi_flash,
        &pin_flash_cs_n,
        &pin_flash_mosi,
        &pin_flash_miso,
        &pin_flash_sck,
        10 );

    spi_flash_create( &board.dev_flash, &board.spi_flash );

    pp_printf("SPI Flash RDID = %x\n", spi_flash_read_id( &board.dev_flash ) );

    bb_i2c_init( &board.i2c_clka_distr, &pin_ad9520_clka_scl, &pin_ad9520_clka_sda );
    bb_i2c_init( &board.i2c_clkb_distr, &pin_ad9520_clkb_scl, &pin_ad9520_clkb_sda );

    ad9520_init( &board.dev_clka_distr, &board.i2c_clka_distr, 0x5c );
    ad9520_init( &board.dev_clkb_distr, &board.i2c_clkb_distr, 0x5c );
    
    ertm14_dds_sync_test();

}




