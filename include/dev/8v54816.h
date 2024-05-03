/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Harvey Leicester <harvey.leicester@cern.ch>
 * Based on https://gitlab.cern.ch/twlostow/openmmc
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

#ifndef __8V54816_h
#define __8V54816_h

#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/bb_i2c.h"

#define CP_PORT_OUT     (uint8_t)1<<7
#define CP_PORT_IN      (uint8_t)0
#define CP_TERM_ON      (uint8_t)1<<6
#define CP_TERM_OFF     (uint8_t)0
#define CP_POLARITY_P   (uint8_t)1<<5
#define CP_POLARITY_N   (uint8_t)0
#define CP_SRC_MASK     (uint8_t)0x0F
#define CP_NUM_CH       (uint8_t)16

enum CP_CLK_SRC{
    CH0,
    CH1,
    CH2,
    CH3,
    CH4,
    CH5,
    CH6,
    CH7,
    CH8,
    CH9,
    CH10,
    CH11,
    CH12,
    CH13,
    CH14,
    CH15
};

struct wr_8v54816_interface_device
{
  struct i2c_bus master;  
  struct gpio_pin pin_scl;
  struct gpio_pin pin_sda;
  struct gpio_device gpio_i2c;
  void *base_addr;
  uint8_t i2c_addr;
  uint8_t config[CP_NUM_CH];
};

int crosspoint_8v54816_configure(struct wr_8v54816_interface_device *dev);
void wr_crosspoint_8v54816_init(struct wr_8v54816_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, int scl, int sda);
int crosspoint_8v54816_configure_gen(void *dev);

#endif
