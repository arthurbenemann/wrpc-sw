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

#include "board.h"
#include "dev/ad9910.h"

#include "hw/wb_dds_sync_unit.h"

#define DDS_SYNC_ENABLED 0x1
#define DDS_SYNC_NEGATIVE 0x2
#define DDS_SYNC_USE_EXT_FINE_DELAY 0x4

#define DDS_SYNC_N_CHANNELS 4

struct dds_sync_unit_channel {
    uint32_t flags;
    int pps_offset_ps;
    int index;
    int delay_tap_size;
    int (*set_external_delay)( struct dds_sync_unit_channel* ch, int n_taps );
};

struct dds_sync_unit_device {
    void* base;
    struct dds_sync_unit_channel channels[DDS_SYNC_N_CHANNELS];
};

void dds_sync_unit_create( struct dds_sync_unit_device *dev, uint32_t base )
{
    int i;
    dev->base = (void*) base;
    writel( 0, dev->base + DS_REG_CSR ); // disable core
    for(i=0;i<DDS_SYNC_N_CHANNELS;i++)
    {
        dev->channels[i].delay_tap_size = 2000 / 31;
        dev->channels[i].index = i;
    }
}

void dds_sync_unit_setup_channel ( struct dds_sync_unit_device* dev, int ch, int enable, int pps_offset_ps, int polarity )
{
    dev->channels[ch].flags = (enable ? DDS_SYNC_ENABLED : 0 );
    dev->channels[ch].pps_offset_ps = pps_offset_ps;
    
    if(polarity)
        dev->channels[ch].flags |= DDS_SYNC_NEGATIVE;
}

void dds_sync_unit_set_external_fine_delay ( struct dds_sync_unit_device* dev, int ch, int tap_size,  int (*set_external_delay)( struct dds_sync_unit_channel* ch, int ) )
{
    dev->channels[ch].flags |= DDS_SYNC_USE_EXT_FINE_DELAY;
    dev->channels[ch].delay_tap_size = tap_size;
    dev->channels[ch].set_external_delay = set_external_delay;
}

void dds_sync_unit_trigger( struct dds_sync_unit_device* dev )
{
    int i;
    uint32_t trig_mask = 0;

    for(i = 0 ; i < DDS_SYNC_N_CHANNELS; i++ )
    {
        struct dds_sync_unit_channel* ch = &dev->channels[i];

        if(ch->flags & DDS_SYNC_ENABLED)
        {
            uint32_t ocr;
            int polarity = ch->flags & DDS_SYNC_NEGATIVE;
            uint32_t coarse_par = ch->pps_offset_ps / 16000; // refclk period = 16 ns = 16000 ps
            uint32_t coarse_ser = ch->pps_offset_ps / 2000 - coarse_par * 8;
            uint32_t fine = (ch->pps_offset_ps % 2000) / ch->delay_tap_size;
            uint32_t mask = 0xff; //(1 << (7-coarse_ser));
            
            ocr = (coarse_par << DS_OCR0_PPS_OFFS_SHIFT)
	                | (mask << DS_OCR0_MASK_SHIFT)
                    | (fine << DS_OCR0_FINE_SHIFT)
                    | (polarity ? DS_OCR0_POL : 0 );

            //pp_printf("Channel %d OCR %x\n", c->index, ocr );
      

            writel( ocr, dev->base + DS_REG_OCR0 + 4 * i); // configure

            if(ch->flags & DDS_SYNC_USE_EXT_FINE_DELAY)
            {
                ch->set_external_delay( ch, fine );
            }

            trig_mask |= (1<< i);
        }
    }

 //   pp_printf("TrigMask %x\n", trig_mask );

    writel( trig_mask, dev->base + DS_REG_CSR); // arm trigger
}

int dds_sync_unit_poll( struct dds_sync_unit_device* dev )
{
    uint32_t rv = readl( dev->base + DS_REG_CSR);
    int i;

    for(i = 0 ; i < DDS_SYNC_N_CHANNELS; i++ )
    {
        struct dds_sync_unit_channel* ch = &dev->channels[i];

        uint32_t mask = 1 << ( DS_CSR_READY_SHIFT + i);

        if( (ch->flags & DDS_SYNC_ENABLED) && ((rv & mask) == 0 ) )
            return 0;
    }

    return 1;
}

static void ad9910_set_fine_delay( struct dds_sync_unit_channel *ch, int n_taps )
{
    pp_printf("SetFD ch %d taps %d\n", ch->index, n_taps );
}

static struct dds_sync_unit_device dds_sync_dev;

#define ERTM14_DDS_SYNC_DDS_REF 0
#define ERTM14_DDS_SYNC_DDS_LO 1
#define ERTM14_DDS_SYNC_CLKA 2
#define ERTM14_DDS_SYNC_CLKB 3

int ertm14_dds_sync_init()
{
    static int ch_delays[] = { 100000, 100000, 100000, 100000 };

    dds_sync_unit_create( &dds_sync_dev, BASE_ERTM14_DDS_SYNC_UNIT );

    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_DDS_REF, 1, ch_delays[0], 0 );
    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_DDS_LO, 1, ch_delays[1], 0 );
    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_CLKA, 1, ch_delays[2], 1 );
    dds_sync_unit_setup_channel ( &dds_sync_dev, ERTM14_DDS_SYNC_CLKB, 1, ch_delays[3], 1 );

    dds_sync_unit_set_external_fine_delay( &dds_sync_dev, ERTM14_DDS_SYNC_DDS_REF, 75, ad9910_set_fine_delay );
    dds_sync_unit_set_external_fine_delay( &dds_sync_dev, ERTM14_DDS_SYNC_DDS_LO, 75, ad9910_set_fine_delay );
}

extern struct ad9910_device dds_ad9910_ref;
extern struct ad9910_device dds_ad9910_lo;

void ertm14_dds_sync_test()
{
    shw_pps_gen_init();

    shw_pps_gen_enable_output(1);
    shw_pps_gen_unmask_output(1);

    ertm14_dds_sync_init();
    int i = 0;

    ad9910_configure_sync( &dds_ad9910_ref, 1, 0 );
    ad9910_configure_sync( &dds_ad9910_lo, 1, 0 );

    for(;;)
    {
        pp_printf("Trig! [%d]\n", i++);
        dds_sync_unit_trigger( &dds_sync_dev );
        //pp_printf("Poll!\n");
        while(!dds_sync_unit_poll( &dds_sync_dev ));
    }
}