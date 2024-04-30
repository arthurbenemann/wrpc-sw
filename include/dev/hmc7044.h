/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Quentin Genoud Duvillaret <quentin.genoud@cern.ch>
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

#ifndef __HMC7044_H
#define __HMC7044_H

#include <stdint.h>

#include "dev/gpio.h"
#include "dev/simple_spi.h"

struct hmc7044_config_reg {
    uint16_t addr;
    uint8_t value;
};

struct hmc7044_config {
    int n_regs;
    struct hmc7044_config_reg regs[];
};

struct hmc7044_device {
    struct simple_spi_device *bus;
    struct gpio_pin *pin_clk_sel;
    struct gpio_pin *pin_reset;
    struct gpio_pin *pin_sync;
    struct gpio_pin *pin_gpio1;
    struct gpio_pin *pin_gpio2;
};


void hmc7044_write(struct hmc7044_device *dev, uint16_t reg, uint8_t value);
uint16_t hmc7044_read(struct hmc7044_device *dev, uint16_t reg);
int hmc7044_configure(struct hmc7044_device *dev, struct hmc7044_config *cfg);
int hmc7044_init(struct hmc7044_device *dev, struct simple_spi_device *spi, struct gpio_pin *pin_reset, struct gpio_pin *pin_clk_sel,
    struct gpio_pin *pin_sync, struct gpio_pin *pin_gpio1, struct gpio_pin *pin_gpio2);

#endif
