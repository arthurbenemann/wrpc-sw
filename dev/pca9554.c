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

#include "dev/i2c.h"
#include "dev/pca9554.h"

uint8_t pca9554_read_reg( struct pca9554_gpio_device *dev, uint8_t reg )
{
	uint8_t rv;
	int err;
	bb_i2c_start(dev->bus);
	err |= bb_i2c_put_byte(dev->bus, dev->i2c_addr << 1);
	bb_i2c_put_byte(dev->bus,  reg );
	bb_i2c_repeat_start(dev->bus );
	err |= bb_i2c_put_byte(dev->bus,  (dev->i2c_addr << 1) | 1);
	bb_i2c_get_byte(dev->bus, &rv, 1 );
	bb_i2c_stop(dev->bus);

	if( err )
	{
		board_dbg("pca9554 not responding [addr = 0x%x]!\n", dev->i2c_addr );
	}

	return rv;
}

void pca9554_write_reg( struct pca9554_gpio_device *dev, uint8_t reg, uint8_t value )
{
	bb_i2c_start(dev->bus);
	bb_i2c_put_byte(dev->bus, dev->i2c_addr << 1);
	bb_i2c_put_byte(dev->bus,  reg );
	bb_i2c_put_byte(dev->bus,  value );
	bb_i2c_stop(dev->bus);
}

void pca9554_gpio_out(const struct gpio_pin *pin, int value)
{
	struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;


	uint8_t oreg = pca9554_read_reg( dev, PCA9554_REG_OUT );

	if( value )
		oreg |= ( 1<< pin->pin );
	else
		oreg &= ~ ( 1<< pin->pin );


	pca9554_write_reg( dev, PCA9554_REG_OUT, oreg );
}



void pca9554_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
	struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;


	uint8_t dreg = pca9554_read_reg( dev, PCA9554_REG_CONFIG );

	if( ! dir )
		dreg |= ( 1<< pin->pin );
	else
		dreg &= ~ ( 1<< pin->pin );


	pca9554_write_reg( dev, PCA9554_REG_CONFIG, dreg );
	
}


int pca9554_gpio_in(const struct gpio_pin *pin)
{
	//struct pca9554_gpio_device* dev = ( struct pca9554_gpio_device* ) pin->device->priv;

// fixme: implement
	return 0;
}


void pca9554_gpio_init( struct pca9554_gpio_device *dev, struct i2c_bus *bus, uint8_t i2c_addr )
{
	dev->bus = bus;
	dev->i2c_addr = i2c_addr;
	dev->gpio.priv = (void *) dev;
	dev->gpio.read_pin = pca9554_gpio_in;
	dev->gpio.set_dir = pca9554_gpio_set_dir;
	dev->gpio.set_out = pca9554_gpio_out;
}
