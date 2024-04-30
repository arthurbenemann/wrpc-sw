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

#include "pp-printf.h"
#include "wrc-debug.h"
#include "dev/gpio.h"
#include "dev/bb_spi.h"
#include "dev/ad5061.h"

#define FRAME_LENGTH    24

#define OUTPUT_FULL     ((2 << 15) - 1)
#define OUTPUT_MID      ((2 << 14) - 1)

#define OUTPUT_DEFAULT  0

/* Default config */
static struct ad5061_shift_reg ad5061_reg = {
    .power_mode = (uint8_t)MODE_NORMAL,
    .value = OUTPUT_DEFAULT
};

void ad5061_write(struct ad5061_device *dev, struct ad5061_shift_reg *reg)
{
    uint64_t to_write = (uint64_t)(((reg->power_mode & 0x03) << 16) | reg->value);

    bb_spi_cs(dev->bus, 0);
    bb_spi_write(dev->bus, to_write, FRAME_LENGTH);
    bb_spi_cs(dev->bus, 1);
}

void ad5061_set_output(struct ad5061_device *dev, uint16_t value)
{
    ad5061_reg.value = value;
    ad5061_write(dev, &ad5061_reg);
}

void ad5061_set_mode(struct ad5061_device *dev, enum ad5061_modes mode)
{
    ad5061_reg.power_mode = (uint8_t)mode;
    ad5061_write(dev, &ad5061_reg);
}

void ad5061_probe(struct ad5061_device *dev, struct spi_bus *bus)
{
    dev->bus = bus;

    /* Set default config (in case probe is called a 2nd time)*/
    ad5061_reg.power_mode = (uint8_t)MODE_NORMAL;
    ad5061_reg.value = OUTPUT_DEFAULT;

    /* AD5061 selected when SYNC is low */
    bb_spi_cs(dev->bus, 1);
    /* Let AD5061 some time to invalidate transaction in case CS was low */
    bb_spi_delay(dev->bus);

    /* Write the default config */
    ad5061_write(dev, &ad5061_reg);
}

#if 0
void ad9910_trigger_update(struct ad9910_device *dev)
{
    dev->trigger_io_update(dev);
}

#define AD9910_REG_CFR1 1
#define AD9910_DEFAULT_CFR1  0x400820

int ad9910_probe( struct ad9910_device *dev, struct spi_bus *bus, void (*trigger_io_update)(struct ad9910_device *dev) )
{
    dev->bus = bus;
    dev->trigger_io_update = trigger_io_update;

    bb_spi_cs(dev->bus, 0);

    ad9910_write( dev, 0, 0x02000002, 32); // unidir mode for SDIO
    ad9910_trigger_update( dev );

    uint32_t id = ad9910_read( dev, AD9910_REG_CFR1, 32 );
    dev_dbg("AD9910 ID[%p]: 0x%x (expected 0x%x)\n", dev, id, AD9910_DEFAULT_CFR1 );

    return (id == AD9910_DEFAULT_CFR1) ? 0 : -1;
}

uint64_t ad9910_frequency_to_ftw( uint64_t freq_hz )
{
    return  (1ULL << 32) * freq_hz / AD9910_REF_FREQ;
}

uint64_t ad9910_ftw_to_frequency( uint64_t ftw )
{
    return  (ftw * (uint64_t) AD9910_REF_FREQ ) >> 32;
}

int ad9910_program( struct ad9910_device *dev, uint64_t ftw_n, int phase, int fs_current )
{
    int i;

    // formula (2) from AD9910 datasheet, page 22

    uint64_t ftw = ftw_n;
    uint64_t prof0_cr = ftw | (0x8b5ULL << 48); 

    dev_dbg("ad9910 FTW = %d\n", (uint32_t) ftw);

//    dev_dbg("ad9910_program [%08x%08x] asf %d!\n", (uint32_t)(prof0_cr >> 32), (uint32_t)prof0_cr, asf );

    for(i = 0; ad9910_default_config[i].addr >= 0; i++)
    {
        struct ad9910_config_reg r = ad9910_default_config[i];

        if(r.addr == 3)
        {
            r.value &= 0xffffff00ULL;
            r.value |= (uint64_t) fs_current;     // Aux DAC control: DAC Full scale current
        } else
        if( r.addr == 0xe ) // profile 0
            r.value = prof0_cr;

        ad9910_write( dev, r.addr, r.value, r.nbits );
    }

    ad9910_trigger_update( dev );
    return 0;
}

#define AD9910_SYNC_VERIF_DELAY_TAPS 0

void ad9910_configure_sync( struct ad9910_device *dev, int enable, int fine_delay_taps )
{
    uint32_t r10 = ((fine_delay_taps & 0x1f) << 3)
                    | (enable ? ( 1<<27) : 0)
                    | (AD9910_SYNC_VERIF_DELAY_TAPS << 28) | (1<<26);

    ad9910_write( dev, 0xa, 0 , 32 );
    ad9910_write( dev, 0x1, 0x00000820, 32);              // CFR2, TW - enabled sync pulse timing validation
    ad9910_trigger_update( dev );

    ad9910_write( dev, 0x1, 0x00000800, 32);              // CFR2, TW - enabled sync pulse timing validation
    ad9910_write( dev, 0xa, r10 , 32 );
    ad9910_trigger_update( dev );
}

void ad9910_enable_external_ioupdate( struct ad9910_device *dev, int enable )
{
    uint64_t cfr2 = ad9910_read(dev, 1, 32 );

    pp_printf("CFR2 = 0x%08x\n", (unsigned) cfr2 );

    if( enable )
        cfr2 &= ~ (1<<23);
    else
        cfr2 |= (1<<23);

    ad9910_write( dev, 1, cfr2, 32 );
}
#endif