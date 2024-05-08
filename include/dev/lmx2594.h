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

#ifndef __LMX2594_H
#define __LMX2594_H

#include <stdint.h>

#include "dev/gpio.h"
#include "dev/simple_spi.h"

#define MUXOUT_LD_SEL   (1<<2)
#define MUXOUT_MODE_RB  (uint8_t)0 //readback
#define MUXOUT_MODE_LD  (uint8_t)1 //lock detect
#define MUXOUT_MODE_NULL (uint8_t)2 

struct lmx2594_config_reg {
    uint16_t addr;
    uint16_t value;
};

struct lmx2594_config {
    int n_regs;
    struct lmx2594_config_reg regs[];
};

struct lmx2594_device {
    struct simple_spi_device *bus;
    struct gpio_pin *pin_sync;
    struct gpio_pin *pin_muxout_ld;
    uint8_t muxout_mode;
    uint16_t r0;
};

int lmx2594_configure(struct lmx2594_device *dev, struct lmx2594_config *cfg);
int lmx2594_init(struct lmx2594_device *dev, struct simple_spi_device *spi, uint32_t spi_base, struct gpio_pin *pin_sync, struct gpio_pin *pin_muxout_ld);
void lmx2594_setmuxout(struct lmx2594_device *dev, uint8_t muxout_mode);

#endif