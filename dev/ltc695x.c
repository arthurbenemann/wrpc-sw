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
#include <sys/errno.h>

#include "dev/ltc695x.h"

int ltc695x_init( struct ltc695x_device *dev, struct spi_bus *bus )
{
    dev->bus = bus;
    return 0;
}

// Read from LTC695x via SPI
uint8_t ltc695x_read(struct ltc695x_device *dev, uint32_t reg) {
    uint8_t rv;
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1) | 1, 8);
    rv = bb_spi_read(dev->bus, 8);
    bb_spi_cs(dev->bus, 0);

    return rv;
}

void ltc695x_write(struct ltc695x_device *dev, uint32_t reg, uint8_t value) {
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1), 8);
    bb_spi_write(dev->bus, value, 8);
    bb_spi_cs(dev->bus, 0);
};

#define LTC695x_R0_LOCK (1<<2)

int ltc695x_configure(struct ltc695x_device *dev, struct ltc695x_config* cfg)
{
    int i;

    for(i = 0; i < cfg->n_regs; i++) {
        ltc695x_write(dev, cfg->regs[i].addr, cfg->regs[i].value);
    }

    return 0;
}

#define LTC6953_PD_NORMAL (0)
#define LTC6953_PD_MUTE   (1)
#define LTC6953_PD_OUTPUT (2)
#define LTC6953_PD_OUTPUT_AND_DIVIDER (3)

#define LTC6953_OR0_MP_DIV_MASK (0xf8)
#define LTC6953_OR0_MP_DIV_SHIFT (3)

#define LTC6953_OR0_MD_DIV_MASK (0x7)
#define LTC6953_OR0_MD_DIV_SHIFT (0)

#define LTC6953_OR1_SRQEN (1<<7)
#define LTC6953_OR1_OINV (1<<4)

#define LTC6953_OR1_MODE_MASK (0x60)
#define LTC6953_OR1_MODE_SHIFT (5)

#define LTC6953_OR1_MODE_

int ltc6953_set_pdown( struct ltc695x_device *dev, int out, int pd )
{
    int shift = (out & 0x3) * 2;
    int reg = 0x3 + (out / 4);

    uint8_t r = ltc695x_read(dev, reg);

    r &= ~( 0x3 << shift );
    r |= pd << shift;

    //dev_dbg("ltc6953 out %d [addr %x mask %x r %x] PD = %d\n", out, reg, shift, r, pd );

    ltc695x_write(dev, reg, r );
    return 0;
}


int ltc6953_enable_output( struct ltc695x_device *dev, int output, int enabled )
{
    return ltc6953_set_pdown( dev, output, enabled ? LTC6953_PD_NORMAL : LTC6953_PD_OUTPUT );
}


int ltc6953_configure_output( struct ltc695x_device *dev, int output, int divider, int invert )
{
    uint8_t div_mp, div_md;

    
    //dev_dbg("ltc6953 out %d div=%d inv=%d\n", output, divider, invert );
    
    switch(divider)
    {
        case 1: div_mp = 0; div_md = 1; break;
        case 2: div_mp = 1; div_md = 1; break;
        case 4: div_mp = 3; div_md = 1; break;
        case 8: div_mp = 7; div_md = 1; break;
        case 16: div_mp = 15; div_md = 1; break;
        case 10: div_mp = 9; div_md = 1; break;
        default: return -EINVAL; // unsupported divider
    }

    uint8_t or0 = (div_mp << LTC6953_OR0_MP_DIV_SHIFT) | (div_md << LTC6953_OR0_MD_DIV_SHIFT);
    uint8_t or1 = invert ? LTC6953_OR1_OINV : 0;

    int base = (output * 4 + 0xc);

    //dev_dbg("ltc6953 r%02x = %02x\n", base+0, or0 );
    //dev_dbg("ltc6953 r%02x = %02x\n", base+1, or1 );
    ltc695x_write( dev, base + 0, or0 );
    ltc695x_write( dev, base + 1, or1 );

    return 0;
}