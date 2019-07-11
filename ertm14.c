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
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/ertm15_rf_distr.h"
#include "dev/i2c.h"



#define BASE_AUXWB 0x28000
#define BASE_CLOCK_MONITOR  0x28100

struct gpio_device gpio_aux;

struct wb_clock_monitor_device ertm14_cmon;


static const struct gpio_pin pin_pll_main_cs_n = { &gpio_aux, 0 };
static const struct gpio_pin pin_pll_main_sdi = { &gpio_aux, 1 };
static const struct gpio_pin pin_pll_main_sdo = { &gpio_aux, 2 };
static const struct gpio_pin pin_pll_main_sclk = { &gpio_aux, 3 };
static const struct gpio_pin pin_pll_main_reset = { &gpio_aux, 4 };
static const struct gpio_pin pin_pll_main_lock = { &gpio_aux, 5 };

static const struct gpio_pin pin_pll_ext_cs_n = { &gpio_aux, 6 };
static const struct gpio_pin pin_pll_ext_sdi = { &gpio_aux, 7 };
static const struct gpio_pin pin_pll_ext_sdo = { &gpio_aux, 8 };
static const struct gpio_pin pin_pll_ext_sclk = { &gpio_aux, 9 };
static const struct gpio_pin pin_pll_ext_reset = { &gpio_aux, 10 };
static const struct gpio_pin pin_pll_ext_lock = { &gpio_aux, 11 };

static const struct gpio_pin pin_mac_addr_scl = { &gpio_aux, 13 };
static const struct gpio_pin pin_mac_addr_sda = { &gpio_aux, 12 };

static const struct gpio_pin pin_main_xo_en_n = { &gpio_aux, 14 };

static const struct gpio_pin pin_ltc6950_sclk = { &gpio_aux, 15 };
static const struct gpio_pin pin_ltc6950_sdi = { &gpio_aux, 16 };
static const struct gpio_pin pin_ltc6950_sdo = { &gpio_aux, 17 };
static const struct gpio_pin pin_ltc6950_ce_gen = { &gpio_aux, 18 };
static const struct gpio_pin pin_ltc6950_ce_distr = { &gpio_aux, 19 };
static const struct gpio_pin pin_ltc6950_sync = { &gpio_aux, 20 };

static const struct gpio_pin pin_ad9910_lo_sdio = { &gpio_aux, 4+21 };
static const struct gpio_pin pin_ad9910_lo_sclk = { &gpio_aux, 5+21 };
static const struct gpio_pin pin_ad9910_lo_reset = { &gpio_aux, 6+21 };
static const struct gpio_pin pin_ad9910_lo_io_update = { &gpio_aux, 3+21 };

static const struct gpio_pin pin_ad9910_ref_sdio = { &gpio_aux, 4+28 };
static const struct gpio_pin pin_ad9910_ref_sclk = { &gpio_aux, 5+28 };
static const struct gpio_pin pin_ad9910_ref_reset = { &gpio_aux, 6+28 };
static const struct gpio_pin pin_ad9910_ref_io_update = { &gpio_aux, 3+28 };

static const struct gpio_pin pin_ocxo_override = { &gpio_aux, 48 };
static const struct gpio_pin pin_ocxo_cs_n = { &gpio_aux, 51 };
static const struct gpio_pin pin_ocxo_sclk = { &gpio_aux, 50 };
static const struct gpio_pin pin_ocxo_data = { &gpio_aux, 49 };

static const struct gpio_pin pin_pwrmon_adc_cs_n = {  &gpio_aux, 52 };
static const struct gpio_pin pin_pwrmon_adc_dout = {  &gpio_aux, 46 };
static const struct gpio_pin pin_pwrmon_adc_din = {  &gpio_aux, 47 };
static const struct gpio_pin pin_pwrmon_adc_sclk = {  &gpio_aux, 45 };


struct spi_bus spi_pll_main;
struct spi_bus spi_pll_ext;
struct spi_bus spi_ltc6950;
struct spi_bus spi_ad9910_ref;
struct spi_bus spi_ad9910_lo;
struct spi_bus spi_ocxo_dac;
struct spi_bus spi_ad7888;

struct ad951x_device ad9516_main;
struct ad951x_device ad9516_ext;
struct ltc6950_device ltc6950_pll;
struct ad9910_device dds_ad9910_ref;
struct ad9910_device dds_ad9910_lo;
struct ad7888_device pwrmon_adc;
struct ertm15_rf_distribution_device rf_distr;

struct i2c_bus i2c_mac_addr;

struct m24aa025_device m24_mac_ids[2];

static struct ad951x_config pll_main_dot050_config =
#include "ertm_14_pll_main_dot050_config.h"

static struct ad951x_config pll_main_ocxo_config =
#include "ertm_14_pll_ocxo_config.h"

static struct ltc6950_config pll_ertm15_config =
#include "ertm_15_ltc6950_config.h"


void ertm14_init()
{
    int i;
    uint32_t id;

    wb_gpio_create( &gpio_aux, BASE_AUXWB );

    gen_gpio_set_dir(&pin_main_xo_en_n, 1);
    gen_gpio_out(&pin_main_xo_en_n, 0);


    bb_spi_create ( &spi_pll_main,
        &pin_pll_main_cs_n,
        &pin_pll_main_sdi,
        &pin_pll_main_sdo,
        &pin_pll_main_sclk,
        AD951X_BIT_DELAY
        );

    
    bb_spi_create ( &spi_pll_ext,
        &pin_pll_ext_cs_n,
        &pin_pll_ext_sdi,
        &pin_pll_ext_sdo,
        &pin_pll_ext_sclk,
        AD951X_BIT_DELAY
        );

    
    bb_spi_create( &spi_ltc6950,
        &pin_ltc6950_ce_gen,
        &pin_ltc6950_sdi,
        &pin_ltc6950_sdo,
        &pin_ltc6950_sclk,
        100 );

    
   bb_spi_create( &spi_ad9910_ref,
        NULL,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sclk,
        100 );


bb_spi_create( &spi_ad9910_lo,
        NULL,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sclk,
        100 );



    ltc6950_pll.bus = &spi_ltc6950;

    id = ltc6950_read( &ltc6950_pll, 0x16 );
    
    if( id != 0x65 )
    {
        pp_printf("Error initializing LTC6950 (read RevID: 0x%x, expected: 0x%x)\n", id, 0x65 );
    }

    ad951x_init(&ad9516_main, &spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock );
    ad951x_init(&ad9516_ext, &spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock );

    //ad951x_configure(&ad9516_main, &pll_main_dot050_config);
    ad951x_configure(&ad9516_main, &pll_main_ocxo_config);
 
    ltc6950_configure(&ltc6950_pll, &pll_ertm15_config);

    wb_cm_init(&ertm14_cmon, BASE_CLOCK_MONITOR, 5);
    wb_cm_configure(&ertm14_cmon, 0, 2, 6250000 );
    wb_cm_restart(&ertm14_cmon);

    gen_gpio_out(&pin_ocxo_override, 0);

    bb_spi_create( &spi_ocxo_dac,
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

    dds_ad9910_ref.pin_ioupdate = &pin_ad9910_ref_io_update;
    dds_ad9910_lo.pin_ioupdate = &pin_ad9910_lo_io_update;

    ad9910_probe( &dds_ad9910_ref, &spi_ad9910_ref );
    ad9910_probe( &dds_ad9910_lo, &spi_ad9910_lo );

    usleep(1000000);
    ad9910_program(&dds_ad9910_ref, 0, 0, 0);

    
    bb_i2c_init( &i2c_mac_addr, &pin_mac_addr_scl, &pin_mac_addr_sda );
    m24aa025_init( &m24_mac_ids[0], &i2c_mac_addr, 0x50 );
    m24aa025_init( &m24_mac_ids[1], &i2c_mac_addr, 0x51 );

    uint8_t mac[6];

    m24aa025_read_mac( &m24_mac_ids[0], mac );
    ep_set_mac_addr( mac );

    bb_spi_create( &spi_ad7888,
        &pin_pwrmon_adc_cs_n,
        &pin_pwrmon_adc_din,
        &pin_pwrmon_adc_dout,
        &pin_pwrmon_adc_sclk,
        100 );
    
    ad7888_create( &pwrmon_adc, &spi_ad7888 );
    
    ertm15_rf_distr_init( &rf_distr, &pwrmon_adc );

    for(;;)
    {
        ertm15_rf_distr_measure_power( &rf_distr );
        //ad7888_poll( &pwrmon_adc );

    }

    

}




