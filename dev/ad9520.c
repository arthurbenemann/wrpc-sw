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

#include "dev/ad9520.h"
#include "dev/i2c.h"


// Write to AD9510 via SPI
void ad9520_write(struct ad9520_device *dev, uint32_t reg, uint8_t value)
{
    #if 0
    if ( reg == 0x0 && !( value == 0x90 || value == 0xb0 ) )
    {
        pp_printf("Warning, some nasty programmer trying to disable AD9520's bidirectional SPI mode!\n");
        // prevent from chaging the SPI config (SDIO is bidirectional, changing this will cause the chip to stop responding!)
        value |= 0x80;
    }
    #endif

    bb_i2c_start( dev->bus );
    bb_i2c_put_byte( dev->bus, dev->addr << 1 );
    bb_i2c_put_byte( dev->bus, reg >> 8);
    bb_i2c_put_byte( dev->bus, reg & 0xff);
    bb_i2c_put_byte( dev->bus, value );
    bb_i2c_stop( dev->bus );
}

// Read from AD9510 via SPI
uint8_t ad9520_read(struct ad9520_device *dev, uint32_t reg) {
    uint8_t rv;
    bb_i2c_start( dev->bus );
    int ack = bb_i2c_put_byte( dev->bus, dev->addr << 1 );
    bb_i2c_put_byte( dev->bus, reg >> 8);
    bb_i2c_put_byte( dev->bus, reg & 0xff);
    bb_i2c_repeat_start( dev->bus );
    bb_i2c_put_byte( dev->bus, (dev->addr << 1) | 1 );
    bb_i2c_get_byte( dev->bus, &rv, 1);
    bb_i2c_stop( dev->bus );
    return rv;
}


int ad9520_init(struct ad9520_device *dev, struct i2c_bus *bus, uint8_t addr)
{
    dev->bus = bus;
    dev->addr = addr;

    pp_printf("AD9520 init!\n");

    ad9520_write( dev, 0x00, 0x81);  // unidir mode
   	ad9520_write( dev, 0x232, 0x01);  // commit

    int id = ad9520_read( dev, 0x3 );
    
   

    return 0;
}


#if 0

void ad9510_soft_reset(struct spi_bus *bus) {
    int reg;
    
    // set reset bit to zero, to one, and to zero again
    ad9510_write(bus, 0x00, 0x90 );
    ad9510_write(bus, 0x00, 0xb0 );
    ad9510_write(bus, 0x00, 0x90 );
}

// Configure AD9510
int ad9510_configure(struct spi_bus *bus, struct ad95xx_config *cfg) {
    int i;

    if(cfg->regs[cfg->n_regs].addr != -1) {
        pp_printf("WARNING! AD9510 config regs don't end with an end-of-list marker. Did you make mistake counting the number of regs\n");
    }

    ad9510_soft_reset(bus);

    for(i = 0; i < cfg->n_regs; i++) {
        ad9510_write(bus, cfg->regs[i].addr, cfg->regs[i].value);
    }

    for(i = 0; i < cfg->n_regs; i++) {
        int rdbk = ad9510_read(bus, cfg->regs[i].addr);
    }

    return 0;
}


void ad9510_init() {
    int i;

    // Configure SPI bus to AD9510
    for(i = 0; i < 2; i++) {
        spi_init(&bus_ad9510[i]);
        ad9510_write(&bus_ad9510[i], 0x00, 0x90);  // bidir mode, long command
   	    ad9510_write(&bus_ad9510[i], 0x5a, 0x00);  // commit
    }

    // Configure AD9510
    ad9510_configure(&bus_ad9510[0], &default_config);
    ad9510_configure(&bus_ad9510[1], &default_config);
}

#endif
