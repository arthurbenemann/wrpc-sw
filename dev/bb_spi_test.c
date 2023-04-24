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

#include "board.h"
#include "pp-printf.h"
#include "dev/gpio.h"
#include "dev/bb_spi.h"

void bb_spi_test(struct spi_bus *bus)
{
    pp_printf("Testing SPI bus: CS = 1 pulse, SCK = 2 pulses, MOSI = 3 pulses\n");
    for(;;)
    {
        gen_gpio_out( bus->pin_cs, 1 );
        gen_gpio_out( bus->pin_cs, 0 );

        gen_gpio_out( bus->pin_sck, 1 );
        gen_gpio_out( bus->pin_sck, 0 );
        gen_gpio_out( bus->pin_sck, 1 );
        gen_gpio_out( bus->pin_sck, 0 );

        gen_gpio_out( bus->pin_mosi, 1 );
        gen_gpio_out( bus->pin_mosi, 0 );
        gen_gpio_out( bus->pin_mosi, 1 );
        gen_gpio_out( bus->pin_mosi, 0 );
        gen_gpio_out( bus->pin_mosi, 1 );
        gen_gpio_out( bus->pin_mosi, 0 );
    }
}
