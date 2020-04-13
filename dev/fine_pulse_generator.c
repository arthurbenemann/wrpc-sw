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

/* fine_pulse-gen - driver for DDS Sync Unit (fine pulse generator) */

#include "board.h"

#include "hw/wb_fine_pulse_gen.h"
#include "dev/fine_pulse_generator.h"

#define FPG_CSR_FORCE0_OFFSET 8 // fixme

void fine_pulse_gen_create( struct fine_pulse_gen_device *dev, uint32_t base )
{
    int i;
    dev->base = (void*) base;

    writel( FPG_CSR_PLL_RST | FPG_CSR_SERDES_RST, dev->base + FPG_REG_CSR ); // reset pll
    usleep(1);
    writel( FPG_CSR_SERDES_RST, dev->base + FPG_REG_CSR ); // unreset pll, keep serdes in reset until PLL locked

    do {
        usleep(10);
    } while( ! ( readl( dev->base + FPG_REG_CSR) & FPG_CSR_PLL_LOCKED ) );
    
    writel( 0, dev->base + FPG_REG_CSR ); // PLL locked? release serdes reset

    
    for(i=0;i<FINE_PULSE_GEN_MAX_CHANNELS;i++)
    {
        dev->channels[i].delay_tap_size = 78 /* ps */;
        dev->channels[i].index = i;
    }
}

void fine_pulse_gen_setup_channel ( struct fine_pulse_gen_device* dev, int ch, int enable, int pps_offset_ps, int flags )
{
    dev->channels[ch].flags = flags;
    dev->channels[ch].pps_offset_ps = pps_offset_ps;

    if( enable )
        dev->channels[ch].flags |= FINE_PULSE_GEN_ENABLED;
    else
        dev->channels[ch].flags &= ~FINE_PULSE_GEN_ENABLED;

    int polarity = flags & FINE_PULSE_GEN_NEGATIVE;

    uint32_t ocr = (polarity ? FPG_OCR0_POL : 0 );

    writel( ocr, dev->base + FPG_REG_OCR0 + 4 * ch); // configure

}

void fine_pulse_gen_set_external_fine_delay ( struct fine_pulse_gen_device* dev, int ch, int tap_size,  int (*set_external_delay)( struct fine_pulse_gen_channel* ch, int ) )
{
    dev->channels[ch].flags |= FINE_PULSE_GEN_USE_EXT_FINE_DELAY;
    dev->channels[ch].delay_tap_size = tap_size;
    dev->channels[ch].set_external_delay = set_external_delay;
}

void fine_pulse_gen_force_pulse( struct fine_pulse_gen_device* dev, int channel )
{
    struct fine_pulse_gen_channel* ch = &dev->channels[channel];

    int polarity = ch->flags & FINE_PULSE_GEN_NEGATIVE;

    uint32_t ocr = (1 << FPG_OCR0_PPS_OFFS_SHIFT)
	                | (0x0 << FPG_OCR0_MASK_SHIFT)
                    | (0 << FPG_OCR0_FINE_SHIFT)
                    | (polarity ? FPG_OCR0_POL : 0 );

    writel( ocr, dev->base + FPG_REG_OCR0 + 4 * channel); // configure


    uint32_t trig_mask = ( 1 << ( channel + FPG_CSR_FORCE0_OFFSET) );

//    pp_printf("ForceSync ch %x ocr %x mask %x\n", channel, ocr, trig_mask);
    

    writel( trig_mask, dev->base + FPG_REG_CSR ); // configure
}

static uint8_t rotr( uint8_t x, int n )
{
    return (x >> n) | (x << (8-n) );
}

void fine_pulse_gen_trigger( struct fine_pulse_gen_device* dev, uint32_t mask, int force_now )
{
    int i;
    uint32_t trig_mask = 0;

    for(i = 0 ; i < FINE_PULSE_GEN_MAX_CHANNELS; i++ )
    {
        struct fine_pulse_gen_channel* ch = &dev->channels[i];

        if( (ch->flags & FINE_PULSE_GEN_ENABLED) && ( mask & (1<<i)))
        {
            uint32_t ocr;
            int polarity = ch->flags & FINE_PULSE_GEN_NEGATIVE;
            int continuous = ch->flags & FINE_PULSE_GEN_CONTINUOUS;
            
            uint32_t coarse_par = ch->pps_offset_ps / 16000; // refclk period = 16 ns = 16000 ps
            uint32_t coarse_ser = ch->pps_offset_ps / 2000 - coarse_par * 8;
            uint32_t fine = (ch->pps_offset_ps % 2000) / ch->delay_tap_size;
            
            uint32_t mask = coarse_ser; // 24/09 VHDL generates mask internally 
            
            pp_printf("trigger: ch %d coarse %d %d flags %x\n", i, coarse_par, coarse_ser, ch->flags );

            ocr = (coarse_par << FPG_OCR0_PPS_OFFS_SHIFT)
	                | (mask << FPG_OCR0_MASK_SHIFT)
                    | (fine << FPG_OCR0_FINE_SHIFT)
                    | (polarity ? FPG_OCR0_POL : 0 )
                    | (continuous ? FPG_OCR0_CONT : 0 );

            if( ch->flags & FINE_PULSE_GEN_USE_EXT_TRIGGER )
                ocr |= FPG_OCR0_TRIG_SEL;

            writel( ocr, dev->base + FPG_REG_OCR0 + 4 * i); // configure

            if(ch->flags & FINE_PULSE_GEN_USE_EXT_FINE_DELAY)
            {
                ch->set_external_delay( ch, fine );
            }

            ch->flags |= FINE_PULSE_GEN_CH_ARMED;

            trig_mask |= (1<<i);
        }
    }

 //   pp_printf("TrigMask %x\n", trig_mask );

    if(force_now)
        trig_mask <<= FPG_CSR_FORCE0_OFFSET; // fixme: use bitshifts from header file

    writel( trig_mask, dev->base + FPG_REG_CSR); // arm trigger
}

int fine_pulse_gen_is_triggered( struct fine_pulse_gen_device* dev, uint32_t mask )
{
    uint32_t rv = readl( dev->base + FPG_REG_CSR);
    int i;

    for(i = 0 ; i < FINE_PULSE_GEN_MAX_CHANNELS; i++ )
    {
        if( (mask & (1<<i)) == 0 )
            continue;

        struct fine_pulse_gen_channel* ch = &dev->channels[i];

        uint32_t mask = 1 << ( FPG_CSR_READY_SHIFT + i);

        if( (ch->flags & FINE_PULSE_GEN_CH_ARMED) )
        {
            if( (rv & mask) == 0 )
                return 0;
            else {
                ch->flags &= ~FINE_PULSE_GEN_CH_ARMED;
            }
        }
    }

    return 1;
}

int fine_pulse_gen_is_armed( struct fine_pulse_gen_device* dev, int ch )
{
    return (dev->channels[ch].flags & FINE_PULSE_GEN_CH_ARMED) ? 1 : 0;
} 
