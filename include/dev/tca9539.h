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

#ifndef __TCA9539_h
#define __TCA9539_h

#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/bb_i2c.h"

#define GPIO_EXP_IP0   0x00
#define GPIO_EXP_IP1   0x01
#define GPIO_EXP_OP0   0x02
#define GPIO_EXP_OP1   0x03
#define GPIO_EXP_POL0  0x04
#define GPIO_EXP_POL1  0x05
#define GPIO_EXP_CFG0  0x06
#define GPIO_EXP_CFG1  0x07

struct wr_tca9539_interface_device
{
  struct i2c_bus master;
  void *base_addr;
  uint8_t i2c_addr;
  struct gpio_pin pin_scl;
  struct gpio_pin pin_sda;
  struct gpio_device gpio_i2c;
  uint16_t *gpi_pins;
  uint8_t num_gpi;
  uint16_t *gpo_pins;
  uint8_t num_gpo;
  uint16_t gpi;
  uint16_t gpo;
};

int gpioexp_tca9539_configure(struct wr_tca9539_interface_device *dev);
void wr_gpioexp_tca9539_init(struct wr_tca9539_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, int scl, int sda, uint16_t *gpi_pins, uint8_t num_gpi, uint16_t *gpo_pins, uint8_t num_gpo);
int gpioexp_tca9539_configure_gen(void *dev);
int wr_gpioexp_tca9539_get_gpi(struct wr_tca9539_interface_device *dev, uint16_t pin);
void wr_gpioexp_tca9539_set_gpo(struct wr_tca9539_interface_device *dev, uint16_t pin, uint16_t state);
int wr_gpioexp_tca9539_set_gpo_gen(void *dev, uint16_t *pin, uint16_t state);

#endif
