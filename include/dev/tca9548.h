/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Harvey Leicester <harvey.leicester@cern.ch>
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

#ifndef __TCA9548_h
#define __TCA9548_h

#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/bb_i2c.h"

#define DEV_PER_CH    8
#define NUM_CHANNELS  8


struct wr_tca9548_interface_device
{
  void *base_addr;
  uint8_t i2c_addr;
  struct gpio_pin pin_scl;
  struct gpio_pin pin_sda;
  struct gpio_device gpio_i2c;
  struct i2c_bus master;
  uint8_t active_ch;
  uint8_t (*device)[DEV_PER_CH];  
};

void wr_mux_tca9548_init(struct wr_tca9548_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, int scl, int sda);
void wr_mux_tca9548_discover(struct wr_tca9548_interface_device *dev);
void wr_mux_tca9548_set_channel(struct wr_tca9548_interface_device *dev, uint8_t channel);
int wr_mux_tca9548_add_device(struct wr_tca9548_interface_device *dev, uint8_t device_addr, uint8_t device_channel);
int wr_mux_tca9548_access(struct wr_tca9548_interface_device *dev, void *access_dev, uint8_t slave_addr, int (*accessor)(void *dev));
int wr_mux_tca9548_access_wr(struct wr_tca9548_interface_device *dev, void *access_dev, uint8_t slave_addr, uint8_t *data, uint8_t no_bytes, int (*accessor)(void *dev, uint8_t *data, uint8_t no_bytes));

#endif