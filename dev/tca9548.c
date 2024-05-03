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
#include "dev/tca9548.h"

#define AUX_I2C_PIN_SCL 1<<8
#define AUX_I2C_PIN_SDA 1<<9

#define GPIO_REG_CODR 0x00000000
#define GPIO_REG_SODR 0x00000004
#define GPIO_REG_DDR  0x00000008
#define GPIO_REG_PSR  0x0000000C

static uint8_t devices[NUM_CHANNELS][DEV_PER_CH] = {{0x00}};

static void wr_mux_tca9548_gpio_out(const struct gpio_pin *pin, int value)
{
  struct wr_tca9548_interface_device* dev = ( struct wr_tca9548_interface_device* ) pin->device->priv;
  uint32_t reg = GPIO_REG_DDR;

  uint32_t bit = (pin->pin); // == AUX_I2C_PIN_SCL ? AUX_I2C_PIN_SCL : AUX_I2C_PIN_SDA);
  uint32_t status = readl(dev->base_addr + reg);
  status = value ? (status | bit) : (status & ~bit);

  writel(status, dev->base_addr + reg);
}


static void wr_mux_tca9548_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
  wr_mux_tca9548_gpio_out(pin, !dir);
}


static int wr_mux_tca9548_gpio_in(const struct gpio_pin *pin)
{
  struct wr_tca9548_interface_device* dev = (struct wr_tca9548_interface_device* ) pin->device->priv;

  uint32_t gpi = readl(dev->base_addr + GPIO_REG_PSR);
  return (gpi & pin->pin ? 1 : 0);
}

void wr_mux_tca9548_set_channel(struct wr_tca9548_interface_device *dev, uint8_t channel)
{
  if((channel < NUM_CHANNELS) && (channel != dev->active_ch)){
    bb_i2c_start(&dev->master);
    bb_i2c_put_byte(&dev->master, dev->i2c_addr << 1);
    bb_i2c_put_byte(&dev->master, 0xFF & (1 << (channel)));
    bb_i2c_stop(&dev->master);
    dev->active_ch = channel;
  }
}

static int wr_mux_tca9548_get_channel(struct wr_tca9548_interface_device *dev, uint8_t addr){

  for(int ch=0; ch<NUM_CHANNELS; ch++){
    for(int slv=0; slv<DEV_PER_CH; slv++){
      if(dev->device[ch][slv] == addr){
        return ch;
      }
    }
  }
  return -1;  //no device found
}

int wr_mux_tca9548_add_device(struct wr_tca9548_interface_device *dev, uint8_t device_addr, uint8_t channel)
{

  if(channel < NUM_CHANNELS){
    for(uint8_t i=0; i<DEV_PER_CH; i++){      
      if(dev->device[channel][i] == 0x80){
        dev->device[channel][i] = device_addr;
        return 0;
      }
    }
  }
  board_dbg("wr_mux_tca9548_add_device() failed to add device 0x%x channel %i\n", device_addr, channel);
  return -1;
}

void wr_mux_tca9548_discover(struct wr_tca9548_interface_device *dev)
{
  //reuse bb_i2c_scan but store discovered devices
  int i;
  int ch;
  for(ch=0; ch<NUM_CHANNELS; ch++){
    board_dbg("Scan channel %i\n", ch);
    wr_mux_tca9548_set_channel(dev, ch);
    for (i=0; i<0x80; i++) {
         bb_i2c_start(&dev->master);
      if ((!bb_i2c_put_byte(&dev->master, i<<1)) && (i != dev->i2c_addr)){
        board_dbg("found : 0x%x\n", i);        
        wr_mux_tca9548_add_device(dev, i, ch);
      }
      bb_i2c_stop(&dev->master);
    }
  }
} 

int wr_mux_tca9548_access(struct wr_tca9548_interface_device *dev, void *access_dev, uint8_t slave_addr, int (*accessor)(void *dev)){

  int ch = wr_mux_tca9548_get_channel(dev, slave_addr);
  if(ch < 0){
    return -1;
  }
   wr_mux_tca9548_set_channel(dev, ch);
   return accessor(access_dev);
}

int wr_mux_tca9548_access_wr(struct wr_tca9548_interface_device *dev, void *access_dev, uint8_t slave_addr, uint8_t *data, uint8_t no_bytes, int (*accessor)(void *dev, uint8_t *data, uint8_t no_bytes)){

  int ch = wr_mux_tca9548_get_channel(dev, slave_addr);
  if(ch < 0){
    return -1;
  }
  
  wr_mux_tca9548_set_channel(dev, ch);
  accessor(access_dev, data, no_bytes);
  return 0;
}

void wr_mux_tca9548_init(struct wr_tca9548_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr, int scl, int sda)
{
  dev->base_addr = (void *) base_addr;
  dev->gpio_i2c.priv = (void *) dev;
  dev->gpio_i2c.read_pin = wr_mux_tca9548_gpio_in;
  dev->gpio_i2c.set_dir = wr_mux_tca9548_gpio_set_dir;
  dev->gpio_i2c.set_out = wr_mux_tca9548_gpio_out;
  dev->i2c_addr = i2c_addr;
  dev->pin_scl.device = &dev->gpio_i2c;
  dev->pin_scl.pin = scl;
  dev->pin_sda.device = &dev->gpio_i2c;
  dev->pin_sda.pin = sda;
  dev->device = devices;
  dev->active_ch = 0xFF;
  wr_mux_tca9548_gpio_out(&(dev->pin_scl), 1);
  wr_mux_tca9548_gpio_out(&(dev->pin_sda), 1);
  bb_i2c_create(&dev->master, &dev->pin_scl, &dev->pin_sda);

  for(uint8_t i=0; i<NUM_CHANNELS; i++){
    for(uint8_t j=0; j<DEV_PER_CH; j++){
      dev->device[i][j] = 0x80;
    }
  }
}

