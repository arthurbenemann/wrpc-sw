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

#include "dev/ltc6950.h"

// Read from LTC6950 via SPI
uint8_t ltc6950_read(struct ltc6950_device *dev, uint32_t reg) {
    uint8_t rv;
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1) | 1, 8);
    rv = bb_spi_read(dev->bus, 8);
    bb_spi_cs(dev->bus, 0);
    return rv;
}

void ltc6950_write(struct ltc6950_device *dev, uint32_t reg, uint8_t value) {
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1), 8);
    bb_spi_write(dev->bus, value, 8);
    bb_spi_cs(dev->bus, 0);
};

#define LTC6950_R0_LOCK (1<<2)

int ltc6950_configure(struct ltc6950_device *dev, struct ltc6950_config* cfg)
{
    int i;

    for(i = 0; i < cfg->n_regs; i++) {
      //  pp_printf("LTC write %x %x\n", cfg->regs[i].addr, cfg->regs[i].value);
        ltc6950_write(dev, cfg->regs[i].addr, cfg->regs[i].value);
    }

    //for(;;)
    {
        uint8_t r0 = ltc6950_read( dev, 0 );
        //pp_printf("tlc r0 = %x\n", r0 );
        timer_delay_ms(100);
    }
}