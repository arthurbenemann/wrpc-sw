/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BB_I2C_H
#define __BB_I2C_H

#include <stdint.h>

struct i2c_bus
{
    struct gpio_pin *pin_scl;
    struct gpio_pin *pin_sda;
    int loop_delay;
};

uint8_t bb_i2c_devprobe(struct i2c_bus *bus, uint8_t i2c_addr);
void bb_i2c_create(struct i2c_bus *bus, struct gpio_pin *pin_scl, struct gpio_pin *pin_sda );
void bb_i2c_init(struct i2c_bus *bus);
void bb_i2c_start(struct i2c_bus *bus);
void bb_i2c_repeat_start(struct i2c_bus *bus);
void bb_i2c_stop(struct i2c_bus *bus);
void bb_i2c_get_byte(struct i2c_bus *bus, uint8_t *data, uint8_t last);
uint8_t bb_i2c_put_byte(struct i2c_bus *bus, uint8_t data);
void bb_i2c_delay(uint32_t delay);
void bb_i2c_scan(struct i2c_bus *bus);

#endif
