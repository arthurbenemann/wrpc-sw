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

#ifndef __LTC6950_H
#define __LTC6950_H

#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"

struct ltc6950_device {
    struct spi_bus *bus;
};

struct ltc6950_config_reg {
    uint16_t addr;
    uint8_t value;
};

struct ltc6950_config {
    int n_regs;
    struct ltc6950_config_reg regs[];
};


int ltc6950_init( struct ltc6950_device *dev, struct spi_bus *bus );
uint8_t ltc6950_read(struct ltc6950_device *dev, uint32_t reg);
void ltc6950_write(struct ltc6950_device *dev, uint32_t reg, uint8_t value);
int ltc6950_configure(struct ltc6950_device *dev, struct ltc6950_config* cfg);

#endif
