/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2022 Nikhef (www.Nikhef.nl)
 * Author: Peter Jansweijer <peterj@nikhef.nl> based on work
 * from Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
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

#include "board.h"
#include "wrc.h"
#include "dev/bb_spi.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/i2c_eeprom.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "storage.h"
#include "softpll_ng.h"

struct babywr_board board;

static struct gpio_pin pin_eeprom_scl        = { &board.gpio_aux, 0 };
static struct gpio_pin pin_eeprom_sda        = { &board.gpio_aux, 1 };
static struct gpio_pin pin_aux_scl           = { &board.gpio_aux, 2 };
static struct gpio_pin pin_aux_sda           = { &board.gpio_aux, 3 };

int babywr_init()
{
    /* most of the I/Os of the slow peripherals (i2c, spi) are bitbanged. First, let's
       initialize the GPIO controller they're connected to */
    wb_gpio_create( &board.gpio_aux, BASE_GPIO );

    return 0;
}

struct i2c_bus            i2c_wrc_eeprom;
struct i2c_bus            dev_i2c_aux;
struct i2c_eeprom_device  wrc_eeprom_dev;
struct i2c_eeprom_device  wrc_uid_dev;

int wrc_board_early_init()
{
    /* Initialize SPEC7 clocking */
    babywr_init();
    
    /* create and initialize eeprom I2C bus */
    bb_i2c_create(&i2c_wrc_eeprom,
         &pin_eeprom_scl,
         &pin_eeprom_sda );
    bb_i2c_init(&i2c_wrc_eeprom);

    /* create and initialize auxiliary I2C bus */
    bb_i2c_create(&dev_i2c_aux,
         &pin_aux_scl,
         &pin_aux_sda );
    bb_i2c_init(&dev_i2c_aux);

    i2c_eeprom_create(&wrc_eeprom_dev, &i2c_wrc_eeprom, FMC_EEPROM_ADR, 2);
    storage_i2ceeprom_create( &wrc_storage_dev, &wrc_eeprom_dev );

    /*
     * Mount SDBFS filesystem from storage.
     */
    storage_mount( &wrc_storage_dev );
    
    /* create and initialize UID eeprom I2C bus */
    i2c_eeprom_create(&wrc_uid_dev, &i2c_wrc_eeprom, UID_EEPROM_ADR, 1);

    return 0;
}

int wrc_board_init()
{
    uint8_t mac_addr[6];
    /*
     * Read MAC addr from Unique-ID, IC D12, 24AA025E48
     */

    i2c_eeprom_read(&wrc_uid_dev, UID_OFFSET , mac_addr, sizeof(mac_addr));
    board_dbg("MAC addr: %x:%x:%x:%x:%x:%x\n",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

    ep_set_mac_addr(&wrc_endpoint_dev, mac_addr);
    ep_pfilter_init_default(&wrc_endpoint_dev);

    return 0;
}

int wrc_board_create_tasks()
{
    return 0;
}
