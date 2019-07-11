/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __I2C_H
#define __I2C_H

#include <stdint.h>

struct i2c_bus
{
    struct gpio_pin *pin_scl;
    struct gpio_pin *pin_sda;
    int loop_delay;
};

uint8_t mi2c_devprobe(uint8_t i2cif, uint8_t i2c_addr);
void mi2c_init(uint8_t i2cif);
void mi2c_start(uint8_t i2cif);
void mi2c_repeat_start(uint8_t i2cif);
void mi2c_stop(uint8_t i2cif);
void mi2c_get_byte(uint8_t i2cif, unsigned char *data, uint8_t last);
unsigned char mi2c_put_byte(uint8_t i2cif, unsigned char data);

void mi2c_delay(uint32_t delay);
//void mi2c_scan(uint8_t i2cif);


uint8_t bb_i2c_devprobe(struct i2c_bus *bus, uint8_t i2c_addr);
void bb_i2c_init(struct i2c_bus *bus, struct gpio_pin *pin_scl, struct gpio_pin *pin_sda );
void bb_i2c_start(struct i2c_bus *bus);
void bb_i2c_repeat_start(struct i2c_bus *bus);
void bb_i2c_stop(struct i2c_bus *bus);
void bb_i2c_get_byte(struct i2c_bus *bus, uint8_t *data, uint8_t last);
uint8_t bb_i2c_put_byte(struct i2c_bus *bus, uint8_t data);
void bb_i2c_delay(uint32_t delay);

#endif
