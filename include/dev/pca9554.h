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

#ifndef __PCA9554_H
#define __PCA9554_H

#include <stdint.h>

#include "dev/bb_i2c.h"

#define PCA9554_REG_IN 0
#define PCA9554_REG_OUT 1
#define PCA9554_REG_INVERT 2
#define PCA9554_REG_CONFIG 3
 
 struct pca9554_gpio_device
{
	struct i2c_bus *bus;
	uint8_t i2c_addr;
	struct gpio_device gpio;
};

uint8_t pca9554_read_reg( struct pca9554_gpio_device *dev, uint8_t reg );
void pca9554_write_reg( struct pca9554_gpio_device *dev, uint8_t reg, uint8_t value );
void pca9554_gpio_out(const struct gpio_pin *pin, int value);
void pca9554_gpio_set_dir(const struct gpio_pin *pin, int dir);
int pca9554_gpio_in(const struct gpio_pin *pin);
void pca9554_gpio_init( struct pca9554_gpio_device *dev, struct i2c_bus *bus, uint8_t i2c_addr );

#endif
