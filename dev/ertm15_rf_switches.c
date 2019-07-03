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

#include "board.h"
#include "dev/gpio.h"
#include "dev/74x595.h"

extern struct gpio_device gpio_aux;

static const struct gpio_pin pin_lo_ctrl_ser = { &gpio_aux, 39 };
static const struct gpio_pin pin_lo_ctrl_updtclk = { &gpio_aux, 40 };
static const struct gpio_pin pin_lo_ctrl_shftclk = { &gpio_aux, 41 };

static const struct gpio_pin pin_ref_ctrl_ser = { &gpio_aux, 42 };
static const struct gpio_pin pin_ref_ctrl_updtclk = { &gpio_aux, 43 };
static const struct gpio_pin pin_ref_ctrl_shftclk = { &gpio_aux, 44 };

static struct gpio_device gpio_rfsw_ref;
// fixme: more than one GPIO chain using '595 register requires dynamic allocator for private data
//static struct gpio_device gpio_rfsw_lo;

#define ERTM15_RF_OUT_ON 0
#define ERTM15_RF_OUT_MONITOR 1
#define ERTM15_RF_OUT_OFF 2

#define ERTM15_RF_LO 0
#define ERTM15_RF_REF 1

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

static struct pin_mapping* find_pins_for_channel ( int path, int channel )
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

int ertm15_rf_switch_set( int path, int channel, int state)
{
    struct gpio_device *gpio = ( path == ERTM15_RF_REF ? &gpio_rfsw_ref : &gpio_rfsw_ref); // fixme
    struct pin_mapping *pins = find_pins_for_channel( path, channel );
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

int ertm15_rf_switch_get( int path, int channel )
{

}


void ertm15_rf_switches_init()
{
    int i;
    x595_gpio_create ( &gpio_rfsw_ref, 3, &pin_ref_ctrl_updtclk, &pin_ref_ctrl_shftclk, NULL, &pin_ref_ctrl_ser );

// disable all RF outputs to the backplane
    for( i = 0; rf_switch_sreg_pin_mapping[i].channel != 0; i++ )
    {
        struct pin_mapping* p = &rf_switch_sreg_pin_mapping[i];

        ertm15_rf_switch_set( p->path, p->channel, ERTM15_RF_OUT_OFF );
    }
}


