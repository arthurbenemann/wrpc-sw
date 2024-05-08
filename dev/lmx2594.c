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

#include <wrc.h>
#include "pp-printf.h"
#include "dev/lmx2594.h"

#define LMX2594_FRAME_LEN   24

static void lmx2594_write(struct lmx2594_device *dev, uint8_t reg, uint16_t value)
{
  uint32_t frame = (((reg << 16) | value) & 0x7fffff); 
  sspi_transfer(dev->bus, LMX2594_FRAME_LEN, frame);
}

static uint16_t lmx2594_read(struct lmx2594_device *dev, uint8_t reg)
{

  if(dev->muxout_mode == MUXOUT_MODE_RB){
    uint32_t frame = (((0x80 | reg) << 16) & 0xff0000);
    return (uint16_t)(sspi_transfer(dev->bus, LMX2594_FRAME_LEN, frame));
  }
  return 0;
}

void lmx2594_setmuxout(struct lmx2594_device *dev, uint8_t muxout_mode){

  if(dev->muxout_mode != muxout_mode){
    dev->r0 = (muxout_mode == MUXOUT_MODE_LD) ? (dev->r0 | MUXOUT_LD_SEL) : (dev->r0 & ~MUXOUT_LD_SEL);
    lmx2594_write(dev, 0x00, dev->r0);
    dev->muxout_mode = muxout_mode;
    gen_gpio_out(dev->pin_muxout_ld, (muxout_mode == MUXOUT_MODE_LD) ? 1 : 0);    //indicate pin function to top level
  }
}

int lmx2594_configure(struct lmx2594_device *dev, struct lmx2594_config *cfg)
{

    //reset
    lmx2594_write(dev, 0x00, 0x2412);
    timer_delay_ms(1);
    lmx2594_write(dev, 0x00, 0x2410);

    //write configuration
    int i = 0;
    for(i=0; i<cfg->n_regs; i++) {
        lmx2594_write(dev, cfg->regs[i].addr, cfg->regs[i].value);
        if(cfg->regs[i].addr == 0){
          dev->r0 = cfg->regs[i].value;   //store r0 settings for later
        }
    }

    //check configuration
    lmx2594_setmuxout(dev, MUXOUT_MODE_RB);
    for(i=0; i<cfg->n_regs; i++){
      if(lmx2594_read(dev, cfg->regs[i].addr) != cfg->regs[i].value){
        return -1;
      }
    }
    
    //set mux out pin to indicate lock
    lmx2594_setmuxout(dev, MUXOUT_MODE_LD);    

    //generate sync pulse todo...
    return 0;
}

int lmx2594_init(struct lmx2594_device *dev, struct simple_spi_device *spi, uint32_t spi_base, struct gpio_pin *pin_sync, struct gpio_pin *pin_muxout_ld)
{
    dev->bus = spi;
    dev->pin_sync = pin_sync;
    dev->pin_muxout_ld = pin_muxout_ld;
    sspi_init(dev->bus, spi_base, 625000, SSPI_POS_EDGE, SSPI_AUTO_SS);
    dev->muxout_mode = MUXOUT_MODE_NULL;
    gen_gpio_set_dir(dev->pin_sync, 1);
    gen_gpio_out(dev->pin_sync, 0);
    gen_gpio_set_dir(dev->pin_muxout_ld, 1);
    gen_gpio_out(dev->pin_muxout_ld, 0);
    return 0;
}