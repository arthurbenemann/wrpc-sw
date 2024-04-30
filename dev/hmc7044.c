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
#include "../boards/wr-switch-v4/board.h"
#include <wrc.h>
#include "hw/wb_spi.h"

#define HMC70144_FRAME_LEN  24
#define AFCZ

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
int hmc7044_init(struct hmc7044_device *dev, struct simple_spi_device *spi, struct gpio_pin *pin_reset, struct gpio_pin *pin_clk_sel,
    struct gpio_pin *pin_sync, struct gpio_pin *pin_gpio1, struct gpio_pin *pin_gpio2)
{
    dev->bus = spi;
    dev->pin_clk_sel = pin_clk_sel;
    dev->pin_reset = pin_reset;
    dev->pin_sync = pin_sync;
    dev->pin_gpio1 = pin_gpio1;
    dev->pin_gpio2 = pin_gpio2;

    /* Initialize HW SPI */
    sspi_init(dev->bus, BASE_SPI, 625000, SSPI_POS_EDGE, SSPI_AUTO_SS);

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


/*
 * Configure HMC7044
 */
int hmc7044_configure(struct hmc7044_device *dev, struct hmc7044_config *cfg) {
    uint8_t value;

    //soft reset
    hmc7044_write(dev, 0x0000, 0x01);
    timer_delay_ms(1);
    hmc7044_write(dev, 0x0000, 0x00);

    hmc7044_write(dev, 0x0054, 0x01); //sdata open drain
    hmc7044_write(dev, 0x0046, 0x00); //gpi1
    hmc7044_write(dev, 0x0047, 0x00); //gpi2
    hmc7044_write(dev, 0x0048, 0x00); //gpi3 (reset value = 0x09 = sleep mode!)
    hmc7044_write(dev, 0x0049, 0x00); //gpi4
    hmc7044_write(dev, 0x0050, 0x00); //gpo1
    hmc7044_write(dev, 0x0051, 0x00); //gpo2
    hmc7044_write(dev, 0x0052, 0x00); //gpo3
    hmc7044_write(dev, 0x0053, 0x00); //gpo4

    //disable all channels
    hmc7044_write(dev, 0x00C8, 0x00);
    hmc7044_write(dev, 0x00D2, 0x00);
    hmc7044_write(dev, 0x00DC, 0x00);
    hmc7044_write(dev, 0x00E6, 0x00);
    hmc7044_write(dev, 0x00F0, 0x00);
    hmc7044_write(dev, 0x00FA, 0x00);
    hmc7044_write(dev, 0x0104, 0x00);
    hmc7044_write(dev, 0x010E, 0x00);
    hmc7044_write(dev, 0x0118, 0x00);
    hmc7044_write(dev, 0x0122, 0x00);
    hmc7044_write(dev, 0x012C, 0x00);
    hmc7044_write(dev, 0x0136, 0x00);
    hmc7044_write(dev, 0x0140, 0x00);
    hmc7044_write(dev, 0x014A, 0x00);

    /*
     * 3. Load config updates (datasheet p.66)
     */
    hmc7044_write(dev, 0x009F, 0x4D);
    hmc7044_write(dev, 0x00A0, 0xDF);
    hmc7044_write(dev, 0x00A5, 0x06);
    hmc7044_write(dev, 0x00A8, 0x06);
    hmc7044_write(dev, 0x00B0, 0x04);

    //global control
    hmc7044_write(dev, 0x0005, 0x00);   //disable sync pin and pll1 ref paths
    //global control
    hmc7044_write(dev, 0x0003, 0x36);   //disable pll1
    //sync control
    hmc7044_write(dev, 0x005B, 0x06);   //defaults

    /*
     * 4. Program PLL2 (VCO range, R2, N2, ref doubler)
     */
    hmc7044_write(dev, 0x0033, 0x01);   // Set R2 = 1 (LSB)
    hmc7044_write(dev, 0x0034, 0x00);   // Set R2 = 1 (MSB)
    hmc7044_write(dev, 0x0035, 0x7D);   // Set N2 = 125 (LSB)
    hmc7044_write(dev, 0x0036, 0x00);   // Set R2 = 125 (MSB)
    hmc7044_write(dev, 0x0032, 0x00);   // Enable freq doubler before R2

    /*
     * 5. Program PLL1
     */
    hmc7044_write(dev, 0x001A, 0x00); //charge pump current (0 as is disabled)

    hmc7044_write(dev, 0x0020, 0x01); // OSCIN input prescaler = 1    
    hmc7044_write(dev, 0x000A, 0x00); // Disable CLKIN0
    hmc7044_write(dev, 0x000B, 0x00); // Configure CLKIN1 (ext 10 MHz ref disable for now)
    hmc7044_write(dev, 0x000C, 0x00); // Disable CLKIN2
    hmc7044_write(dev, 0x000D, 0x00); // Disable CLKIN3
    hmc7044_write(dev, 0x000E, 0x07); // Enable OSCIN (int 10 MHz ref): enable, 100 Ohm + ac coupling

    /*
     * 6. Program SYSREF Timer (set divide ratio, configure pulse generator)
     */
    hmc7044_write(dev, 0x005C, 0xD0); // SYSREF timer LSB (Fvco = 2.5 GHz * 2, SYSREF = 2000 --> Fsysref = 5e9/2000 = 2.5 MHz < 4 MHz)
    hmc7044_write(dev, 0x005D, 0x07); // SYSREF timer MSB

    /*
     * 7. Program the output channels
     */

#ifdef AFCZ

    //afcz config

    //channel 0, REF_CLK, 62.5MHz
    // [7]  : (1) = high perf
    // [6]  : (1) = Can receive SYNC events
    // [5]  : (1) = Can receive SLIP events
    // [3:2]: (00) = Asynchronous startup, (11) = Dynamic startup using pulse generator
    // [1]  : (0) = No auto multislip on startup (1) = Multislip after sync or pulse if slip enable
    // [0]  : (1) = Channel enable    
    hmc7044_write(dev, 0x00C8, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00C9, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00CA, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00CB, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00CC, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00CD, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00CE, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00CF, 0x00);   // Output mux: channel divider
    // [7:6]: Force mute: (00) = Normal, (10) = Forced to logic 0
    // [5]  : (0) = Controlled by enable bit, (1) = Controlled by pulse generator events
    // [4:3]: Mode: (00) = CML, (01) = LVPECL, (10) = LVDS, (11) = CMOS
    // [1:0]: CML driver impedance (per pin): (00) = No resistor, (01) = 100 Ohm, (11) = 50 Ohm
    hmc7044_write(dev, 0x00D0, 0x10);   // LVDS

    //channel 1 WR_CLK, 62.5MHz
    hmc7044_write(dev, 0x00D2, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00D3, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00D4, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00D5, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00D6, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00D7, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00D8, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00D9, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00DA, 0x10);   // LVDS

    //channel 4 SYNC_CLK2, 62.5MHz
    hmc7044_write(dev, 0x00F0, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00F1, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00F2, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00F3, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00F4, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00F5, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00F6, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00F7, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00F8, 0x10);   // LVDS

    // channel 5: 10MHz out
    hmc7044_write(dev, 0x00FA, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00FB, 0xFA);   // Divider (/250) LSB 
    hmc7044_write(dev, 0x00FC, 0x00);   // Divider (/250) MSB 
    hmc7044_write(dev, 0x00FD, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00FE, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00FF, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0100, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0101, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0102, 0x03);   // CML, 50 Ohm     

    //channel 7 SYNC_CLK1, 62.5MHz
    hmc7044_write(dev, 0x010E, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x010F, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0110, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0111, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0112, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0113, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0114, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0115, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0116, 0x10);   // LVDS

    // channel 11: MGTREFCLK2, 125 MHz
    hmc7044_write(dev, 0x0136, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0137, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0138, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0139, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x013A, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x013B, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x013C, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x013D, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x013E, 0x10);   // LVDS      

    // channel 12: AUX_OUT, 62.5MHz
    hmc7044_write(dev, 0x0140, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0141, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0142, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0143, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0144, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0145, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0146, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0147, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0148, 0x03);   // CML, 50 Ohm

    // channel 13: MGTREFCLK1, 125 MHz
    hmc7044_write(dev, 0x014A, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x014B, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x014C, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x014D, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x014E, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x014F, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0150, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0151, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0152, 0x10);   // LVDS    

#else   

    //wrsv4 config

    //channel 0, WR_CLK, 62.5MHz
    hmc7044_write(dev, 0x00C8, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00C9, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00CA, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00CB, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00CC, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00CD, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00CE, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00CF, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00D0, 0x10);   // LVDS   

    //channel 1 REF_CLK, 62.5MHz
    hmc7044_write(dev, 0x00D2, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00D3, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00D4, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00D5, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00D6, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00D7, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00D8, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00D9, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00DA, 0x10);   // LVDS   

    //channel 13 SYNC_CLK1, 62.5MHz
    hmc7044_write(dev, 0x014A, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x014B, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x014C, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x014D, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x014E, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x014F, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0150, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0151, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0152, 0x08);   // LVPECL      

    //channel 11 SYNC_CLK2, 62.5MHz
    hmc7044_write(dev, 0x0136, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0137, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0138, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0139, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x013A, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x013B, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x013C, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x013D, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x013E, 0x08);   // LVPECL    

    //channel 7 EXT_C2M, 62.5MHz
    hmc7044_write(dev, 0x010E, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x010F, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0110, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0111, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0112, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0113, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0114, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0115, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0116, 0x10);   // LVDS   

    //channel 8 62M5_OUT, 62.5MHz
    hmc7044_write(dev, 0x0118, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0119, 0x28);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x011A, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x011B, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x011C, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x011D, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x011E, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x011F, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0120, 0x08);   // LVPECL        

    //channel 10 PLL_OUT, 10MHz
    hmc7044_write(dev, 0x012C, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x012D, 0xFA);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x012E, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x012F, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0130, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0131, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0132, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0133, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0134, 0x08);   // LVPECL         

    // channel 6: GTH_CLK1, 125 MHz
    hmc7044_write(dev, 0x0104, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0105, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0106, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0107, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0108, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0109, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x010A, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x010B, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x010C, 0x10);   // LVDS  

    // channel 12: GTH_CLK0, 125 MHz
    hmc7044_write(dev, 0x0140, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x0141, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x0142, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x0143, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x0144, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x0145, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0146, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0147, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0148, 0x10);   // LVDS     

    // channel 4: GTY_128_CLK0, 125 MHz
    hmc7044_write(dev, 0x00F0, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00F1, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00F2, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00F3, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00F4, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00F5, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00F6, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00F7, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00F8, 0x10);   // LVDS     

    // channel 2: GTY_129_CLK0, 125 MHz
    hmc7044_write(dev, 0x00DC, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00DD, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00DE, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00DF, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00E0, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00E1, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00E2, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00E3, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00E4, 0x10);   // LVDS   

    // channel 5: GTY_130_CLK0, 125 MHz
    hmc7044_write(dev, 0x00FA, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00FB, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00FC, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00FD, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00FE, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00FF, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x0100, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x0101, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x0102, 0x10);   // LVDS         

    // channel 3: GTY_131_CLK0, 125 MHz
    hmc7044_write(dev, 0x00E6, 0xC1);   // High perf, SYNC, normal startup, Ch enable
    hmc7044_write(dev, 0x00E7, 0x14);   // Divider(/40) LSB 
    hmc7044_write(dev, 0x00E8, 0x00);   // Divider (/40) MSB 
    hmc7044_write(dev, 0x00E9, 0x00);   // Fine analog delay
    hmc7044_write(dev, 0x00EA, 0x00);   // Coarse digital delay
    hmc7044_write(dev, 0x00EB, 0x00);   // 12bit multislip digital delay (LSB)
    hmc7044_write(dev, 0x00EC, 0x00);   // 12bit multislip digital delay (MSB)
    hmc7044_write(dev, 0x00ED, 0x00);   // Output mux: channel divider
    hmc7044_write(dev, 0x00EE, 0x10);   // LVDS  

#endif  


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