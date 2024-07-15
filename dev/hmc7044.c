/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Quentin Genoud Duvillaret <quentin.genoud@cern.ch>
 *         Harvey Leicester <harvey.leicester@cern.ch>
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


#include "pp-printf.h"
#include "dev/hmc7044.h"
#include "dev/simple_spi.h"
#include <hw/rawmem.h>
#include <wrc.h>
#include "hw/wb_spi.h"

#define HMC70144_FRAME_LEN  24

/*
 * Write to HMC7044 via SPI
 */
void hmc7044_write(struct hmc7044_device *dev, uint16_t reg, uint8_t value)
{
    /* Build write frame */
    uint32_t frame = (reg << 8) | value;

    /* Start transfer */
    sspi_transfer(dev->bus, HMC70144_FRAME_LEN, frame);
}

/*
 * Read from HMC7044 via SPI
 */
uint16_t hmc7044_read(struct hmc7044_device *dev, uint16_t reg)
{
    /* Build read frame: bit 23: 1 (read), data bits = 0xFF to enable tristate input */
    uint32_t frame = (1 << 23) | (reg << 8) | 0xFF;

    /* Start transfer */
    return (uint8_t)sspi_transfer(dev->bus, HMC70144_FRAME_LEN, frame);
}

/*
 * Initialize HMC7044
 */
int hmc7044_init(struct hmc7044_device *dev, struct simple_spi_device *spi, uint32_t spi_base, struct gpio_pin *pin_reset, struct gpio_pin *pin_clk_sel,
    struct gpio_pin *pin_sync, struct gpio_pin *pin_gpio1, struct gpio_pin *pin_gpio2)
{
    dev->bus = spi;
    dev->pin_clk_sel = pin_clk_sel;
    dev->pin_reset = pin_reset;
    dev->pin_sync = pin_sync;
    dev->pin_gpio1 = pin_gpio1;
    dev->pin_gpio2 = pin_gpio2;

    /* Initialize HW SPI */
    sspi_init(dev->bus, spi_base, 625000, SSPI_POS_EDGE, SSPI_AUTO_SS);

    //Don't toggle reset on init, as if already configured clocks will disappear
    //Reset only toggled in hmc7044_configure()
    // /* 2. Release HW reset */
    // if(dev->pin_reset)
    // {
    //     /* Set n_reset pin as output and toggle it */
    //     gen_gpio_set_dir(dev->pin_reset, 1);
    //     gen_gpio_out(dev->pin_reset, 0);
    //     timer_delay_ms(10); // Needed ?
    //     gen_gpio_out(dev->pin_reset, 1);
    //     timer_delay_ms(10); // Needed ?
    // }
    
    // if(dev->pin_sync)
    // {
    //     gen_gpio_set_dir(dev->pin_sync, 1);
    //     gen_gpio_out(dev->pin_sync, 0);
    // }

    // if(dev->pin_gpio1)
    //     gen_gpio_set_dir(dev->pin_gpio1, 0);
    
    // if(dev->pin_gpio2)
    //     gen_gpio_set_dir(dev->pin_gpio2, 0);

    return 0;
}

int hmc7044_configure(struct hmc7044_device *dev, struct hmc7044_config *cfg) {

    /* 2. Release HW reset */
    if(dev->pin_reset)
    {
        /* Set n_reset pin as output and toggle it */
        gen_gpio_set_dir(dev->pin_reset, 1);
        gen_gpio_out(dev->pin_reset, 0);
        timer_delay_ms(10); // Needed ?
        gen_gpio_out(dev->pin_reset, 1);
        timer_delay_ms(10); // Needed ?
    }


    uint8_t value;

    //soft reset
    hmc7044_write(dev, 0x0000, 0x01);
    timer_delay_ms(1);
    hmc7044_write(dev, 0x0000, 0x00);

    //configure plls
    int i=0;
    for(i=0; i<cfg->n_regs; i++){
        hmc7044_write(dev, cfg->regs[i].addr, cfg->regs[i].value);
    }


    /*
     * 8. Wait until VCO peak detector loop stabilized (~10 ms after 4.)
     */
    timer_delay_ms(10); // Poll for something here instead of blind delay ?

    /*
     * 9. Ensure ref are provided to PLL1 and VCXO powered
     */
    // Should be OK if VCXO is enable prior PLL config

    /*
     * 10. Issue software restart (reset system and initiate cal: toggle restart dividers/FSMs bit to 1 and back to 0)
     */
    // [7]: Reseed request: requests centralized resync timer and FSM to reseed any of the output dividers listening sync events
    // [6]: High perf distr. path: (0) = Power optimized, (1) = Perf optimized
    // [4]: Force holdover
    // [3]: (1) = Mute all output drivers
    // [2]: Pulse generator request: ask for pulse stream
    // [1]: Restart dividers/FSMs (does not affect config)
    // [0]: (1) = Sleep mode
    hmc7044_write(dev, 0x0001, 0x42);   // High perf, restart dividers/FSMs

    timer_delay_ms(1);  // Wait 1 ms to be sure
    
    hmc7044_write(dev, 0x0001, 0x40);   //clear reset

    /*
     * 11. Wait for PLL2 to be locked (~50 µs)
     */
    timer_delay_ms(1);  // Wait 1 ms to be sure

    //check pll 2 state    
    value = hmc7044_read(dev, 0x008c); 
    board_dbg("pll2_autotune_value = 0x%x\n", value); 

    value = hmc7044_read(dev, 0x008d); 
    board_dbg("pll2_error_lsb = 0x%x\n", value); 

    value = hmc7044_read(dev, 0x008e); 
    board_dbg("pll2_status = 0x%x\n", value); 

    value = hmc7044_read(dev, 0x008f); 
    board_dbg("pll2_state = 0x%x\n", value);    

    value = hmc7044_read(dev, 0x0091); 
    board_dbg("sysref_status = 0x%x\n", value);        

    /*
     * 12. Confirm PLL2 is locked by checking PLL2 lock detect bit
     */
    value = hmc7044_read(dev, 0x007D);  // Read alarm readback
    board_dbg("alarm = 0x%x\n", value);

    if(!(value & 0x01))                 // If PLL2_LOCK_DETECT bit is not set, there is a problem
        return -1;

    /*
     * 13. Send sync request (set reseed request bit)
     */
    hmc7044_write(dev, 0x0001, 0xC0);   // Send reseed request, high perf (see 10.)

    timer_delay_ms(1);

    hmc7044_write(dev, 0x0001, 0x40);  //clear request

    /*
     * 14. Wait 6 SYSREF periods (~3 µs)
     */
    timer_delay_ms(1);                  // Wait 1 ms to be sure

    /*
     * 15. Confirm outputs stable (check clock output phase status bit = 1)
     */
    value = hmc7044_read(dev, 0x007D);  // Read alarm readback
    board_dbg("alarm = 0x%x\n", value);    
    if(!(value & 0x04))                 // If clock outputs phase status is not set, there is a problem
        return -2;

    return 0;
}

int hmc7044_checkstatus(struct hmc7044_device *dev){
    uint8_t value = hmc7044_read(dev, 0x007D);  // Read alarm readback
    board_dbg("alarm = 0x%x\n", value);    
    if(!(value & 0x04))                 // phase status
        return -2;

    return 0;
}
