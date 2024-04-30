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

#include <stdint.h>
#include <stdio.h>
#include <sys/errno.h>

#include "dev/8v54816.h"

#include <wrc-debug.h>
#include <hw/rawmem.h>
#include "dev/syscon.h"

#define AUX_I2C_PIN_SCL 1<<8
#define AUX_I2C_PIN_SDA 1<<9

#define GPIO_REG_CODR 0x00000000
#define GPIO_REG_SODR 0x00000004
#define GPIO_REG_DDR  0x00000008
#define GPIO_REG_PSR  0x0000000C

static void crosspoint_8v54816_gpio_out(const struct gpio_pin *pin, int value)
{
  struct wr_8v54816_interface_device* dev = ( struct wr_8v54816_interface_device* ) pin->device->priv;
  uint32_t reg = GPIO_REG_DDR;

  uint32_t bit = (pin->pin == AUX_I2C_PIN_SCL ? AUX_I2C_PIN_SCL : AUX_I2C_PIN_SDA);
  uint32_t status = readl(dev->base_addr + reg);
  status = value ? (status | bit) : (status & ~bit);

  writel(status, dev->base_addr + reg);
}


static void crosspoint_8v54816_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
  crosspoint_8v54816_gpio_out(pin, !dir);
}


static int crosspoint_8v54816_gpio_in(const struct gpio_pin *pin)
{
  struct wr_8v54816_interface_device* dev = (struct wr_8v54816_interface_device* ) pin->device->priv;

  uint32_t gpi = readl(dev->base_addr + GPIO_REG_PSR);

  if ( pin->pin == AUX_I2C_PIN_SCL )
    return (gpi & AUX_I2C_PIN_SCL ? 1 : 0);
  else
    return (gpi & AUX_I2C_PIN_SDA ? 1 : 0);
}

static uint8_t crosspoint_8v54816_read(struct wr_8v54816_interface_device *dev, uint8_t *dst)
{

  bb_i2c_start(&dev->master);
  bb_i2c_put_byte(&dev->master, (dev->i2c_addr << 1) | 0x01);
  
  for(int i = 0; i < CP_NUM_CH; i++){
    bb_i2c_get_byte(&dev->master, &dst[i], i == (CP_NUM_CH - 1) ? 1 : 0);
  }
  bb_i2c_stop(&dev->master);
}

static void crosspoint_8v54816_write(struct wr_8v54816_interface_device *dev)
{
  
  bb_i2c_start(&dev->master);
  bb_i2c_put_byte(&dev->master, dev->i2c_addr << 1);

  //only block transfers are supported
  for(int i=0; i<CP_NUM_CH; i++){
      bb_i2c_put_byte(&dev->master, dev->config[i]);
  }  
  bb_i2c_stop(&dev->master);
}

static void crosspoint_8v54816_setchannel(struct wr_8v54816_interface_device *dev, uint8_t channel, uint8_t config){

  dev->config[channel] = config;

  crosspoint_8v54816_write(dev);
}

//only configures structure, does not write configuration
static void crosspoint_8v54816_configchannel(struct wr_8v54816_interface_device *dev, uint8_t channel, uint8_t config){
  dev->config[channel] = config;
}

static uint8_t crosspoint_8v54816_readchannel(struct wr_8v54816_interface_device *dev, uint8_t channel){

  uint8_t data[CP_NUM_CH] = {0};
  crosspoint_8v54816_read(dev, data);
  return data[channel];
}


int crosspoint_8v54816_configure(struct wr_8v54816_interface_device *dev){

  crosspoint_8v54816_configchannel(dev, CH0, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH1, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH2, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH3, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH4, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH5, CP_PORT_OUT | CP_TERM_ON | CP_POLARITY_P | (CP_SRC_MASK & CH6));
  crosspoint_8v54816_configchannel(dev, CH6, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH7, CP_PORT_OUT | CP_TERM_ON | CP_POLARITY_P | (CP_SRC_MASK & CH6)); //mgt227_0
  crosspoint_8v54816_configchannel(dev, CH8, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH9, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH10, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH11, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH12, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH13, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P);
  crosspoint_8v54816_configchannel(dev, CH14, CP_PORT_OUT | CP_TERM_ON | CP_POLARITY_P | (CP_SRC_MASK & CH15)); //to FPGA
  crosspoint_8v54816_configchannel(dev, CH15, CP_PORT_IN | CP_TERM_ON | CP_POLARITY_P); //ref clk from HMC7044, FMC1

  //select i2c mux channel
  //TODO make seperate TCA9548A struct  
  bb_i2c_start(&dev->master);
  bb_i2c_put_byte(&dev->master, dev->mux_addr << 1);
  bb_i2c_put_byte(&dev->master, 0xFF & (1 << (dev->mux_ch)));
  bb_i2c_stop(&dev->master);
  
  crosspoint_8v54816_write(dev);

  uint8_t data[CP_NUM_CH] = {0};
  crosspoint_8v54816_read(dev, data);

  //sanity check
  for(uint8_t i=0; i<CP_NUM_CH; i++){
    if(dev->config[i] != data[i]){
      return -1;
    }
  }
}


void wr_crosspoint_8v54816_init(struct wr_8v54816_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, uint8_t mux_addr, uint8_t mux_ch)
{

  dev->base_addr = (void *) base_addr;
  dev->gpio_i2c.priv = (void *) dev;
  dev->gpio_i2c.read_pin = crosspoint_8v54816_gpio_in;
  dev->gpio_i2c.set_dir = crosspoint_8v54816_gpio_set_dir;
  dev->gpio_i2c.set_out = crosspoint_8v54816_gpio_out;
  dev->i2c_addr = i2c_addr;
  dev->mux_addr = mux_addr;
  dev->mux_ch = mux_ch;
  dev->pin_scl.device = &dev->gpio_i2c;
  dev->pin_scl.pin = AUX_I2C_PIN_SCL;
  dev->pin_sda.device = &dev->gpio_i2c;
  dev->pin_sda.pin = AUX_I2C_PIN_SDA;
  bb_i2c_create(&dev->master, &dev->pin_scl, &dev->pin_sda);
  //bb_i2c_scan(&dev->master);

  for(uint8_t i=0; i<CP_NUM_CH; i++){
    dev->config[i] = 0;
  }
}