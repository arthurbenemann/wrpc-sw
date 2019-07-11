/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "types.h"
#include "board.h"
#include "syscon.h"
#include "dev/i2c.h"
#include "dev/gpio.h"

void mi2c_delay(uint32_t delay)
{
	int i;
	for (i = 0; i < delay; i++)
		asm volatile ("nop");
}

#define M_SDA_OUT(i, x) { gpio_out(i2c_if[i].sda, x); mi2c_delay(i2c_if[i].loop_delay); }
#define M_SCL_OUT(i, x) { gpio_out(i2c_if[i].scl, x); mi2c_delay(i2c_if[i].loop_delay); }
#define M_SDA_IN(i) gpio_in(i2c_if[i].sda)

void mi2c_start(uint8_t i2cif)
{
	M_SDA_OUT(i2cif, 0);
	M_SCL_OUT(i2cif, 0);
}

void mi2c_repeat_start(uint8_t i2cif)
{
	M_SDA_OUT(i2cif, 1);
	M_SCL_OUT(i2cif, 1);
	M_SDA_OUT(i2cif, 0);
	M_SCL_OUT(i2cif, 0);
}

void mi2c_stop(uint8_t i2cif)
{
	M_SDA_OUT(i2cif, 0);
	M_SCL_OUT(i2cif, 1);
	M_SDA_OUT(i2cif, 1);
}

unsigned char mi2c_put_byte(uint8_t i2cif, unsigned char data)
{
	int i;
	unsigned char ack;

	for (i = 0; i < 8; i++, data <<= 1) {
		M_SDA_OUT(i2cif, data & 0x80);
		M_SCL_OUT(i2cif, 1);
		M_SCL_OUT(i2cif, 0);
	}

	M_SDA_OUT(i2cif, 1);
	M_SCL_OUT(i2cif, 1);

	ack = M_SDA_IN(i2cif);	/* ack: sda is pulled low ->success.     */


	M_SCL_OUT(i2cif, 0);
	M_SDA_OUT(i2cif, 0);

	return ack != 0;
}

void mi2c_get_byte(uint8_t i2cif, unsigned char *data, uint8_t last)
{

	int i;
	unsigned char indata = 0;

	M_SDA_OUT(i2cif, 1);
	/* assert: scl is low */
	M_SCL_OUT(i2cif, 0);

	for (i = 0; i < 8; i++) {
		M_SCL_OUT(i2cif, 1);
		indata <<= 1;
		if (M_SDA_IN(i2cif))
			indata |= 0x01;
		M_SCL_OUT(i2cif, 0);
	}

	if (last) {
		M_SDA_OUT(i2cif, 1);	//noack
		M_SCL_OUT(i2cif, 1);
		M_SCL_OUT(i2cif, 0);
	} else {
		M_SDA_OUT(i2cif, 0);	//ack
		M_SCL_OUT(i2cif, 1);
		M_SCL_OUT(i2cif, 0);
	}

	*data = indata;
}

void mi2c_init(uint8_t i2cif)
{
	M_SCL_OUT(i2cif, 1);
	M_SDA_OUT(i2cif, 1);
}

uint8_t mi2c_devprobe(uint8_t i2cif, uint8_t i2c_addr)
{
	uint8_t ret;
	mi2c_start(i2cif);
	ret = !mi2c_put_byte(i2cif, i2c_addr << 1);
	mi2c_stop(i2cif);

	return ret;
}

//

#undef M_SDA_OUT
#undef M_SCL_OUT
#undef M_SDA_IN


void bb_i2c_delay(uint32_t delay)
{
	int i;
	for (i = 0; i < delay; i++)
		asm volatile ("nop");
}

#define M_SDA_OUT(x) { gen_gpio_out( bus->pin_sda, x); bb_i2c_delay(bus->loop_delay); }
#define M_SCL_OUT(x) { gen_gpio_out( bus->pin_scl, x); bb_i2c_delay(bus->loop_delay); }
#define M_SDA_IN gen_gpio_in(bus->pin_sda)

void bb_i2c_start(struct i2c_bus *bus)
{
	M_SDA_OUT(0);
	M_SCL_OUT(0);
}

void bb_i2c_repeat_start(struct i2c_bus *bus)
{
	M_SDA_OUT(1);
	M_SCL_OUT(1);
	M_SDA_OUT(0);
	M_SCL_OUT(0);
}

void bb_i2c_stop(struct i2c_bus *bus)
{
	M_SDA_OUT(0);
	M_SCL_OUT(1);
	M_SDA_OUT(1);
}

unsigned char bb_i2c_put_byte(struct i2c_bus *bus, uint8_t data)
{
	int i;
	uint8_t ack;

	for (i = 0; i < 8; i++, data <<= 1) {
		M_SDA_OUT(data & 0x80);
		M_SCL_OUT(1);
		M_SCL_OUT(0);
	}

	M_SDA_OUT(1);
	M_SCL_OUT(1);

	ack = M_SDA_IN;	/* ack: sda is pulled low ->success.     */
	M_SCL_OUT(0);
	M_SDA_OUT(0);

	return ack != 0;
}

void bb_i2c_get_byte(struct i2c_bus *bus, uint8_t *data, uint8_t last)
{

	int i;
	uint8_t indata = 0;

	M_SDA_OUT(1);
	/* assert: scl is low */
	M_SCL_OUT(0);

	for (i = 0; i < 8; i++) {
		M_SCL_OUT(1);
		indata <<= 1;
		if (M_SDA_IN)
			indata |= 0x01;
		M_SCL_OUT(0);
	}

	if (last) {
		M_SDA_OUT(1);	//noack
		M_SCL_OUT(1);
		M_SCL_OUT(0);
	} else {
		M_SDA_OUT(0);	//ack
		M_SCL_OUT(1);
		M_SCL_OUT(0);
	}

	*data = indata;
}

void bb_i2c_init(struct i2c_bus *bus,  struct gpio_pin *pin_scl, struct gpio_pin *pin_sda )
{
	bus->pin_scl = pin_scl;
	bus->pin_sda = pin_sda;
	bus->loop_delay = 100;
	M_SCL_OUT(1);
	M_SDA_OUT(1);
}

uint8_t bb_i2c_devprobe(struct i2c_bus *bus, uint8_t i2c_addr)
{
	uint8_t ret;
	bb_i2c_start(bus);
	ret = !bb_i2c_put_byte(bus, i2c_addr << 1);
	bb_i2c_stop(bus);

	return ret;
}

void bb_i2c_scan(struct i2c_bus *bus)
{
    int i;
	pp_printf("Scan\n");
	for(i=0;i<0x80;i++)
    {
    	 bb_i2c_start(bus);
     	if(!bb_i2c_put_byte(bus, i<<1)) pp_printf("found : %x\n", i);
     	bb_i2c_stop(bus);
    }
}