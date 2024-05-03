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


#include <wrc-debug.h>
#include <hw/rawmem.h>
#include "dev/tca9539.h"

#define GPIO_REG_CODR 0x00000000
#define GPIO_REG_SODR 0x00000004
#define GPIO_REG_DDR  0x00000008
#define GPIO_REG_PSR  0x0000000C

static void wr_gpioexp_tca9539_gpio_out(const struct gpio_pin *pin, int value)
{
  struct wr_tca9539_interface_device* dev = ( struct wr_tca9539_interface_device* ) pin->device->priv;
  uint32_t reg = GPIO_REG_DDR;

  uint32_t bit = (pin->pin);
  uint32_t status = readl(dev->base_addr + reg);
  status = value ? (status | bit) : (status & ~bit);

  writel(status, dev->base_addr + reg);
}


static void wr_gpioexp_tca9539_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
  wr_gpioexp_tca9539_gpio_out(pin, !dir);
}


static int wr_gpioexp_tca9539_gpio_in(const struct gpio_pin *pin)
{
  struct wr_tca9539_interface_device* dev = (struct wr_tca9539_interface_device* ) pin->device->priv;

  uint32_t gpi = readl(dev->base_addr + GPIO_REG_PSR);
  return (gpi & pin->pin ? 1 : 0);
}

static void wr_gpioexp_tca9539_write(struct wr_tca9539_interface_device *dev, uint8_t reg, uint8_t value)
{

  bb_i2c_start(&dev->master);
  bb_i2c_put_byte(&dev->master, dev->i2c_addr << 1);
  bb_i2c_put_byte(&dev->master, 0xFF & (reg));
  bb_i2c_put_byte(&dev->master, 0xFF & (value));
  bb_i2c_stop(&dev->master);
}

static void wr_gpioexp_tca9539_read(struct wr_tca9539_interface_device *dev, uint8_t reg, uint8_t *dst)
{
  bb_i2c_start(&dev->master);
  bb_i2c_put_byte(&dev->master, dev->i2c_addr << 1);
  bb_i2c_put_byte(&dev->master, 0xFF & (reg));
  bb_i2c_repeat_start(&dev->master);
  bb_i2c_put_byte(&dev->master, dev->i2c_addr << 1);
  bb_i2c_get_byte(&dev->master, dst, 1);
  bb_i2c_stop(&dev->master);
}


int wr_gpioexp_tca9539_get_gpi(struct wr_tca9539_interface_device *dev, uint16_t pin){

  uint8_t state0 = 0;
  uint8_t state1 = 0;
  wr_gpioexp_tca9539_read(dev, GPIO_EXP_IP0, &state0);
  wr_gpioexp_tca9539_read(dev, GPIO_EXP_IP1, &state1);

  dev->gpi = (state1 << 8) & (state0); 
  return (dev->gpi & pin);
}

int wr_gpioexp_tca9539_set_gpo_gen(void *dev, uint16_t *pin, uint16_t state){

  struct wr_tca9539_interface_device *Dev = (struct wr_tca9539_interface_device *)dev;
  wr_gpioexp_tca9539_set_gpo(Dev, *pin, state);
  return 0;
}

void wr_gpioexp_tca9539_set_gpo(struct wr_tca9539_interface_device *dev, uint16_t pin, uint16_t state){

  dev->gpo = state ? (dev->gpo | pin) : (dev->gpo & ~pin); 

  wr_gpioexp_tca9539_write(dev, GPIO_EXP_OP0, (dev->gpo & 0xFF));
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_OP1, ((dev->gpo >> 8) & 0xFF));
}

int gpioexp_tca9539_configure_gen(void *dev)
{
  struct wr_tca9539_interface_device *Dev = (struct wr_tca9539_interface_device *)dev;
  return gpioexp_tca9539_configure(Dev);
}

int gpioexp_tca9539_configure(struct wr_tca9539_interface_device *dev)
{

  uint8_t gpi0_mask = 0;
  uint8_t gpi1_mask = 0;

  //set to 1 for inputs
  for(uint8_t i=0; i<dev->num_gpi; i++){
    if(dev->gpi_pins[i] >= (1<<8)){
      gpi1_mask |= (dev->gpi_pins[i]) >> 8;
    }else{
      gpi0_mask |= dev->gpi_pins[i];
    }
  }

  //polarity inversion regs
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_POL0, 0x00);
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_POL1, 0x00);

  //config regs
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_CFG0, gpi0_mask);
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_CFG1, gpi1_mask);

  //by default outputs are set...
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_OP0, 0x00);
  wr_gpioexp_tca9539_write(dev, GPIO_EXP_OP1, 0x00);

  return 0;
}

void wr_gpioexp_tca9539_init(struct wr_tca9539_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, int scl, int sda, uint16_t *gpi_pins, uint8_t num_gpi, uint16_t *gpo_pins, uint8_t num_gpo)
{
  dev->base_addr = (void *) base_addr;
  dev->gpio_i2c.priv = (void *) dev;
  dev->gpio_i2c.read_pin = wr_gpioexp_tca9539_gpio_in;
  dev->gpio_i2c.set_dir = wr_gpioexp_tca9539_gpio_set_dir;
  dev->gpio_i2c.set_out = wr_gpioexp_tca9539_gpio_out;
  dev->i2c_addr = i2c_addr;
  dev->pin_scl.device = &dev->gpio_i2c;
  dev->pin_scl.pin = scl;
  dev->pin_sda.device = &dev->gpio_i2c;
  dev->pin_sda.pin = sda;
  dev->gpo = 0;
  dev->gpi = 0;
  dev->gpi_pins = gpi_pins;
  dev->num_gpi = num_gpi;
  dev->gpo_pins = gpo_pins;
  dev->num_gpo = num_gpo;
  wr_gpioexp_tca9539_gpio_out(&(dev->pin_scl), 1);
  wr_gpioexp_tca9539_gpio_out(&(dev->pin_sda), 1);
  bb_i2c_create(&dev->master, &dev->pin_scl, &dev->pin_sda);

}

