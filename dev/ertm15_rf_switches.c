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

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "board.h"
#include "dev/gpio.h"
#include "dev/74x595.h"
#include "dev/ad7888.h"
#include "dev/ertm15_rf_distr.h"

extern struct gpio_device gpio_aux;

static const struct gpio_pin pin_lo_ctrl_ser = { &gpio_aux, 39 };
static const struct gpio_pin pin_lo_ctrl_updtclk = { &gpio_aux, 40 };
static const struct gpio_pin pin_lo_ctrl_shftclk = { &gpio_aux, 41 };

static const struct gpio_pin pin_ref_ctrl_ser = { &gpio_aux, 42 };
static const struct gpio_pin pin_ref_ctrl_updtclk = { &gpio_aux, 43 };
static const struct gpio_pin pin_ref_ctrl_shftclk = { &gpio_aux, 44 };

static struct gpio_device gpio_rfsw_ref;
static struct gpio_device gpio_rfsw_lo;


// mapping between x595 shift reg (IC26..28 on eRTM15 and the RF switch control pins)
// fixme: pin definitions for LO path
static const struct pin_mapping
{
    uint8_t path;
    uint8_t channel;
    uint8_t distr_channel;
    uint8_t ctrl1, ctrl2;
} rf_switch_sreg_pin_mapping[] = {
    {ERTM15_RF_REF, 5, 4, 8 + 6, 8 + 7},    // RF5
    {ERTM15_RF_REF, 4, 5, 8 + 5, 8 + 4},    // RF4
    {ERTM15_RF_REF, 7, 7, 8 + 3, 8 + 2},    // RF5
    {ERTM15_RF_REF, 6, 6, 8 + 1, 8 + 0},    // RF5
    {ERTM15_RF_REF, 10,12, 16 + 7, 16 + 6}, // RF12
    {ERTM15_RF_REF, 9, 9, 6, 7},            // RF9
    {ERTM15_RF_REF, 8, 8, 4, 5},            // RF8
    {ERTM15_RF_REF, 11,11, 2, 3},           // RF11
    {ERTM15_RF_REF, 12,10, 0, 1},           // RF10
    {ERTM15_RF_REF, 0,   0, 0, 0}};

static const struct pin_mapping* find_pins_for_channel ( int path, int channel )
{
    int i;
    for(i=0; rf_switch_sreg_pin_mapping[i].channel !=0; i++ )
    {
        if( rf_switch_sreg_pin_mapping[i].channel == channel && rf_switch_sreg_pin_mapping[i].path == path )
        {
            return &rf_switch_sreg_pin_mapping[i];
        }
    }
    return NULL;
}

static int rf_switch_set( int path, int channel, int state)
{
    struct gpio_device *gpio = ( path == ERTM15_RF_REF ? &gpio_rfsw_ref : &gpio_rfsw_lo);
    const struct pin_mapping *pins = find_pins_for_channel( path, channel );
    struct gpio_pin ctrl1, ctrl2;

    ctrl1.device = gpio;
    ctrl1.pin = pins->ctrl1;
    ctrl2.device = gpio;
    ctrl2.pin = pins->ctrl2;
    

    if(!pins)
        return -1;
    
// HSWA2-30DR+ I/O CTRL pins function:
// CTRL1 = 0, CTRL2 = 0: OFF
// CTRL1 = 0, CTRL2 = 1: MONITOR
// CTRL1 = 1, CTRL2 = 0: ON

    switch(state)
    {
        case ERTM15_RF_OUT_ON:
            gen_gpio_out( &ctrl1, 1 );
            gen_gpio_out( &ctrl2, 0 );
            break;
        case ERTM15_RF_OUT_MONITOR:
            gen_gpio_out( &ctrl1, 0 );
            gen_gpio_out( &ctrl2, 1 );
            break;
        case ERTM15_RF_OUT_OFF:
            gen_gpio_out( &ctrl1, 0 );
            gen_gpio_out( &ctrl2, 0 );
            
        break;
    }
   
    return 0;
}


static void update_rf_switches( struct ertm15_rf_distribution_device *dev )
{
    int i;

    for( i = 0; rf_switch_sreg_pin_mapping[i].channel != 0; i++ )
    {
        struct pin_mapping* p = &rf_switch_sreg_pin_mapping[i];

        int enabled = ( p->path == ERTM15_RF_LO ? dev->lo_enabled : dev->ref_enabled ) & (1 << p->channel );

        pp_printf("rf_distr: switch %s ch %d -> %s\n", p->path == ERTM15_RF_LO ? "LO" : "REF", p->channel, enabled ? "ON" : "OFF" );
        rf_switch_set( p->path, p->channel, enabled ? ERTM15_RF_OUT_ON : ERTM15_RF_OUT_OFF );
    }
}

void ertm15_rf_distr_init( struct ertm15_rf_distribution_device *dev, struct ad7888_device *pwr_mon_adc )
{
    int i;

    x595_gpio_create ( &gpio_rfsw_ref, 3, &pin_ref_ctrl_updtclk, &pin_ref_ctrl_shftclk, NULL, &pin_ref_ctrl_ser );
    x595_gpio_create ( &gpio_rfsw_lo, 3, &pin_lo_ctrl_updtclk, &pin_lo_ctrl_shftclk, NULL, &pin_lo_ctrl_ser );

    dev->pwr_ref_valid = 0;
    dev->pwr_lo_valid = 0;
    dev->ref_enabled = 0;
    dev->lo_enabled = 0;
    dev->pwr_ref_valid = 0;
    dev->pwr_mon_adc = pwr_mon_adc;

    update_rf_switches( dev );
// disable all RF outputs to the backplane
   
}

int convert_power( int adc_value )
{
    //pp_printf("ADCV %d\n", adc_value );

    float adc_voltage = (float) adc_value / 4096.0 * 2.5;
    float rf_power = 10.0 * log( adc_voltage / 2.0 ) / log( 10.0 ) + 15.0; // 2V = 0 dBm, compensate for 15 dB attenuator

    return (int) (rf_power * 100.0);
}

#define ADC_CH_REF_DDS_PA 2
#define ADC_CH_LO_DDS_PA 0
#define ADC_CH_REF_DDS_DISTR 3
#define ADC_CH_LO_DDS_DISTR 1

int ertm15_rf_distr_measure_power ( struct ertm15_rf_distribution_device *dev )
{
    ad7888_start_conversion( dev->pwr_mon_adc, 0x0f );
    while( dev->pwr_mon_adc->channel_valid != 0x0f )
    {
        ad7888_poll( dev->pwr_mon_adc );
        usleep(1000);
    }

    dev->pwr_ref_in = convert_power( dev->pwr_mon_adc->channel[ADC_CH_REF_DDS_PA] );
    dev->pwr_lo_in = convert_power( dev->pwr_mon_adc->channel[ADC_CH_LO_DDS_PA] );


    int i;
    for( i = 4; i <= 12; i ++ )
    {
        dev->pwr_ref_valid &= ~(1<<i);
        if( ! (dev->ref_enabled & (1<<i) ) )
        {
            rf_switch_set( ERTM15_RF_REF, i, ERTM15_RF_OUT_MONITOR );
            usleep(10000);

            ad7888_start_conversion( dev->pwr_mon_adc, 0xff );
            do {
                ad7888_poll( dev->pwr_mon_adc );
            } while ( ! (dev->pwr_mon_adc->channel_valid & ( 1<< ADC_CH_REF_DDS_DISTR ))); 
            dev->pwr_ref_ch[ i ] = convert_power( dev->pwr_mon_adc->channel[ADC_CH_REF_DDS_DISTR] );
            
            pp_printf("Ch REF %d: pwr %d v %d\n", i, dev->pwr_ref_ch[i], dev->pwr_mon_adc->channel_valid );
            dev->pwr_ref_valid |= (1<<i);
            rf_switch_set( ERTM15_RF_REF, i, ERTM15_RF_OUT_OFF );
        }
    }

    return 0;
}

void ertm15_rf_distr_output_enable( struct ertm15_rf_distribution_device *dev, int path, int channel, int enabled )
{
    uint16_t* mask = (path == ERTM15_RF_LO ? &dev->lo_enabled : &dev->ref_enabled );
    
    if( enabled )
        *mask |= ( 1 << channel );
    else
        *mask &= ~( 1 << channel );

    update_rf_switches( dev );
}

