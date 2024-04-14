/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011d CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/*
 * Trivial pll programmer using an spi controller.
 * PLL is AD9516, SPI is opencores
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wrc.h>

#include "board.h"
#include "dev/syscon.h"

static inline void writel(uint32_t data, void *where)
{
     * (volatile uint32_t *)where = data;
}

static inline uint32_t readl(void *where)
{
     return * (volatile uint32_t *)where;
}

struct ad9516_reg {
     uint16_t reg;
     uint8_t val;
};


const struct ad9516_reg ad9516_base_config_mini[] = {
//{0x0000, 0x99},{0x0001, 0x00},{0x0002, 0x10},{0x0003, 0xC3},{0x0004, 0x00},
// PLL power-down = normal operation
{0x0010, 0x7C},

/*
AD9516, DM mode, f_VCO = f_REF / R x (PxB+A) 
AD9516, FD mode, f_VCO = f_REF / R x (PxB) 
// f_VCO should be 1.45 GHz to 1.80GHz
// f_VCO = 25 / 5 x (16x18+12) = 1500MHz -- NORMAL
// f_VCO = 25 / 27 x 16 x 104  = 1540.740740...MHz -- SHINE (DM)
// f_VCO = 25 / 27 x 2 x 832   = 1540.740740...MHz -- SHINE (FD)
// f_VCO = 25 / 2 x (16x4+61) = 1562.5MHz -- 156M25
*/

// NORMAL
// R divider = 5
// {0x0011, 0x05},{0x0012, 0x00},
// A counter = 12
// {0x0013, 0x0C},
// B counter = 18
// {0x0014, 0x12},{0x0015, 0x00},

// SHINE
// R divider = 27
// {0x0011, 0x1b},{0x0012, 0x00},
// A counter = 0
// {0x0013, 0x00},
// B counter = 104
// {0x0014, 0x68},{0x0015, 0x00},

// 156M25
// R divider = 2
{0x0011, 0x02},{0x0012, 0x00},
// A counter = 61
{0x0013, 0x3D},
// B counter = 4
{0x0014, 0x04},{0x0015, 0x00},

// Prescaler P = 16, DM, P could be 2/4/8/16/32
{0x0016, 0x05},

// STATUS pin = REF2 clock, Antibacklash Pulse Width=2.9ns
{0x0017, 0x88},
// NO VCO calibration, Does nothing on SYNC, R/N path delay=0
{0x0018, 0x07},{0x0019, 0x00},
// LD pin = Digital lock detect
{0x001A, 0x00},
// Disable VCO/REF1/REF2 freq monitor, REFMON=GND
{0x001B, 0x00},
// Use REF_SEL pin, REF1 power-on
{0x001C, 0x02},
// PLL status register enable, holdover disabled
{0x001D, 0x00},{0x001E, 0x00},
// Read-only 
// {0x001F, 0x0E},
// OUT6 config, bypasses delay function, default ramp config
{0x00A0, 0x01},{0x00A1, 0x00},{0x00A2, 0x00},
// OUT7 config, bypasses delay function, default ramp config
{0x00A3, 0x01},{0x00A4, 0x00},{0x00A5, 0x00},
// OUT8 config, bypasses delay function, default ramp config
{0x00A6, 0x01},{0x00A7, 0x00},{0x00A8, 0x00},
// OUT9 config, bypasses delay function, default ramp config
{0x00A9, 0x01},{0x00AA, 0x00},{0x00AB, 0x00},

// OUT0/1 config, power down
{0x00F0, 0x0A},{0x00F1, 0x0A},
// OUT2/3 config, power on
{0x00F2, 0x08},{0x00F3, 0x08},
// OUT4/5 config, OUT4 power on, OUT5 power off
{0x00F4, 0x0A},{0x00F5, 0x0A},
// OUT6 config, power on, LVDS
{0x0140, 0x42},
// OUT7 config, power on, LVDS
{0x0141, 0x42},
// OUT8 config, power on, LVDS
{0x0142, 0x42},
// OUT9 config, power on, LVDS
{0x0143, 0x42},

/* Divider 0-2
0x0190/193/196 = M[7:4], N[3:0], D = (N + 1) + (M + 1) = N + M + 2
0x0191/194/197 [7] =  bypass, D = 1
0x192/195/198 [0] = DCCOFF (Duty Cycle and Duty-Cycle Correction) on
*/
// Divider 0, OUT0/OUT1
{0x0190, 0x00},{0x0191, 0x80},{0x0192, 0x00},
// Divider 1, OUT2/OUT3
{0x0193, 0x00},{0x0194, 0x80},{0x0195, 0x00},
// Divider 2, OUT4/OUT5, bypass
{0x0196, 0x00},{0x0197, 0x80},{0x0198, 0x00},

/* Divider 3-4 
0x199/0x19E, divider 3.1, D = N + M + 2
0x19A/0x19F phase offset
0x19B/0x1A0, divider 3.2, D = N + M + 2
0x19C/0x1A1 [4]:1=bypass 3.1/4.1
0x19C/0x1A1 [5]:1=bypass 3.2/4.2
0x19D/0x1A2 [0]:0=DCCOFF on
NORMAL VCO = 1500, IN = 1500/3, OUT6/7 = 1500/3/4=125MHz, OUT8/9=1500/3/4=125MHz
SHINE VCO = 1540.740740, IN = 1540.740740/6, OUT6/7 = 1540.740740/6/2, OUT8/9=1540.740740/6/2=
156M25 VCO = 1562.5, IN = 1562.5/5, OUT6/7 = 1562.5/5/2=156.25MHz, OUT8/9=1562.5/5/2=156.25MHz
*/
// Divider 3.1/3.2, OUT6/OUT7
{0x0199, 0x11},{0x019A, 0x00},{0x019B, 0x00},{0x019C, 0x20},{0x019D, 0x00},
// Divider 4.1/4.2, OUT8/OUT9
{0x019E, 0x11},{0x019F, 0x00},{0x01A0, 0x00},{0x01A1, 0x20},{0x01A2, 0x00},

// NORMAL VCO setting, divider = 3 = 0x01E0(001)
// SHINE VCO setting, divider = 6 = 0x01E0(100)
// 156M25 VCO setting, divider = 5 = 0x01E0(011)
{0x01E0, 0x01},{0x01E1, 0x02},

// System
{0x0230, 0x00}
};

const struct ad9516_reg ad9516_base_config_shine[] = {
//{0x0000, 0x99},{0x0001, 0x00},{0x0002, 0x10},{0x0003, 0xC3},{0x0004, 0x00},
// PLL power-down = normal operation
{0x0010, 0x7C},

/*
AD9516, DM mode, f_VCO = f_REF / R x (PxB+A) 
AD9516, FD mode, f_VCO = f_REF / R x (PxB) 
// f_VCO should be 1.45 GHz to 1.80GHz
// f_VCO = 25 / 5 x (16x18+12) = 1500MHz -- NORMAL
// f_VCO = 25 / 27 x 16 x 104  = 1540.740740...MHz -- SHINE (DM)
// f_VCO = 25 / 27 x 2 x 832   = 1540.740740...MHz -- SHINE (FD)
*/

// NORMAL
// R divider = 5
// {0x0011, 0x05},{0x0012, 0x00},
// A counter = 12
// {0x0013, 0x0C},
// B counter = 18
// {0x0014, 0x12},{0x0015, 0x00},

// SHINE
// R divider = 27
{0x0011, 0x1b},{0x0012, 0x00},
// A counter = 0
{0x0013, 0x00},
// B counter = 104
{0x0014, 0x68},{0x0015, 0x00},

// Prescaler P = 16, DM, P could be 2/4/8/16/32
{0x0016, 0x05},

// STATUS pin = REF2 clock, Antibacklash Pulse Width=2.9ns
{0x0017, 0x88},
// NO VCO calibration, Does nothing on SYNC, R/N path delay=0
{0x0018, 0x07},{0x0019, 0x00},
// LD pin = Digital lock detect
{0x001A, 0x00},
// Disable VCO/REF1/REF2 freq monitor, REFMON=GND
{0x001B, 0x00},
// Use REF_SEL pin, REF1 power-on
{0x001C, 0x02},
// PLL status register enable, holdover disabled
{0x001D, 0x00},{0x001E, 0x00},
// Read-only 
// {0x001F, 0x0E},
// OUT6 config, bypasses delay function, default ramp config
{0x00A0, 0x01},{0x00A1, 0x00},{0x00A2, 0x00},
// OUT7 config, bypasses delay function, default ramp config
{0x00A3, 0x01},{0x00A4, 0x00},{0x00A5, 0x00},
// OUT8 config, bypasses delay function, default ramp config
{0x00A6, 0x01},{0x00A7, 0x00},{0x00A8, 0x00},
// OUT9 config, bypasses delay function, default ramp config
{0x00A9, 0x01},{0x00AA, 0x00},{0x00AB, 0x00},

// OUT0/1 config, power down
{0x00F0, 0x0A},{0x00F1, 0x0A},
// OUT2/3 config, power on
{0x00F2, 0x08},{0x00F3, 0x08},
// OUT4/5 config, OUT4 power on, OUT5 power off
{0x00F4, 0x0A},{0x00F5, 0x0A},
// OUT6 config, power on, LVDS
{0x0140, 0x42},
// OUT7 config, power on, LVDS
{0x0141, 0x42},
// OUT8 config, power on, LVDS
{0x0142, 0x42},
// OUT9 config, power on, CMOS
{0x0143, 0x42},

/* Divider 0-2
0x0190/193/196 = M[7:4], N[3:0], D = (N + 1) + (M + 1) = N + M + 2
0x0191/194/197 [7] =  bypass, D = 1
0x192/195/198 [0] = DCCOFF (Duty Cycle and Duty-Cycle Correction) on
*/
// Divider 0, OUT0/OUT1
{0x0190, 0x00},{0x0191, 0x80},{0x0192, 0x00},
// Divider 1, OUT2/OUT3, bypass, 256.79MHz
{0x0193, 0x00},{0x0194, 0x80},{0x0195, 0x00},
// Divider 2, OUT4/OUT5, bypass
{0x0196, 0x00},{0x0197, 0x80},{0x0198, 0x00},

/* Divider 3-4 
0x199/0x19E, divider 3.1, D = N + M + 2
0x19A/0x19F phase offset
0x19B/0x1A0, divider 3.2, D = N + M + 2
0x19C/0x1A1 [4]:1=bypass 3.1/4.1
0x19C/0x1A1 [5]:1=bypass 3.2/4.2
0x19D/0x1A2 [0]:0=DCCOFF on
VCO = 1540.740, IN = 1540.740/6, OUT6/7 = 1540.740/6/2=128.395MHz, OUT8/9=128.395MHz
*/
// Divider 3.1/3.2, OUT6/OUT7
{0x0199, 0x00},{0x019A, 0x00},{0x019B, 0x00},{0x019C, 0x20},{0x019D, 0x00},
// Divider 4.1/4.2, OUT8/OUT9
{0x019E, 0x00},{0x019F, 0x00},{0x01A0, 0x00},{0x01A1, 0x20},{0x01A2, 0x00},

// VCO setting, divider = 6
{0x01E0, 0x04},{0x01E1, 0x02},

// System
{0x0230, 0x00}
};

const struct ad9516_reg ad9516_ext_base_config[] = {
//{0x0000, 0x99},{0x0001, 0x00},{0x0002, 0x10},{0x0003, 0xC3},{0x0004, 0x00},
// PLL power-down = normal operation
{0x0010, 0x7C},

/*
AD9516, DM mode, f_VCO = f_REF / R x (PxB+A) 
AD9516, FD mode, f_VCO = f_REF / R x (PxB) 
// f_VCO should be 1.45 GHz to 1.80GHz
// f_VCO = 10 / 1 x (8x18+6) = 1500MHz
*/
// R divider = 1
{0x0011, 0x00},{0x0012, 0x00},
// A counter = 6
{0x0013, 0x06},
// B counter = 18
{0x0014, 0x12},{0x0015, 0x00},
// Prescaler P = 8, DM
{0x0016, 0x04},

// STATUS pin = REF1 clock/differential clock, Antibacklash Pulse Width=2.9ns
{0x0017, 0x84},
// NO VCO calibration, Does nothing on SYNC, R/N path delay=0
{0x0018, 0x07},{0x0019, 0x00},
// LD pin = Digital lock detect
{0x001A, 0x00},
// Disable VCO/REF1/REF2 freq monitor, REFMON=GND
{0x001B, 0x00},
// Differential reference mode
{0x001C, 0x01},
// PLL status register enable, holdover disabled
{0x001D, 0x00},{0x001E, 0x00},
// Read-only 
// {0x001F, 0x0E},
// OUT6 config, bypasses delay function, default ramp config
{0x00A0, 0x01},{0x00A1, 0x00},{0x00A2, 0x00},
// OUT7 config, bypasses delay function, default ramp config
{0x00A3, 0x01},{0x00A4, 0x00},{0x00A5, 0x00},
// OUT8 config, bypasses delay function, default ramp config
{0x00A6, 0x01},{0x00A7, 0x00},{0x00A8, 0x00},
// OUT9 config, bypasses delay function, default ramp config
{0x00A9, 0x01},{0x00AA, 0x00},{0x00AB, 0x00},

// OUT0/1 config, power down
{0x00F0, 0x0A},{0x00F1, 0x0A},
// OUT2/3 config, power down
{0x00F2, 0x0A},{0x00F3, 0x0A},
// OUT4/5 config, power down
{0x00F4, 0x0A},{0x00F5, 0x0A},
// OUT6 config, power on, LVDS
{0x0140, 0x42},
// OUT7 config, power off, LVDS
{0x0141, 0x43},
// OUT8 config, power on, CMOS
{0x0142, 0x46},
// OUT9 config, power off, LVDS
{0x0143, 0x43},

/* Divider 0-2
0x0190/193/196 = M[7:4], N[3:0], D = (N + 1) + (M + 1) = N + M + 2
0x0191/194/197 [7] =  bypass, D = 1
0x192/195/198 [0] = DCCOFF (Duty Cycle and Duty-Cycle Correction) on
*/
// Divider 0, OUT0/OUT1
{0x0190, 0x00},{0x0191, 0x80},{0x0192, 0x00},
// Divider 1, OUT2/OUT3
{0x0193, 0x00},{0x0194, 0x80},{0x0195, 0x00},
// Divider 2, OUT4/OUT5, bypass
{0x0196, 0x00},{0x0197, 0x80},{0x0198, 0x00},

/* Divider 3-4 
0x199/0x19E, divider 3.1, D = N + M + 2
0x19A/0x19F phase offset
0x19B/0x1A0, divider 3.2, D = N + M + 2
0x19C/0x1A1 [5]:1=bypass 3.2/4.2
0x19D/0x1A2 [0]:0=DCCOFF on
VCO = 1500, IN = 1500/3, OUT = 1500/3/8=62.5M
*/
// Divider 3.1/3.2, OUT6/OUT7
{0x0199, 0x33},{0x019A, 0x00},{0x019B, 0x11},{0x019C, 0x20},{0x019D, 0x00},
// Divider 4.1/4.2, OUT8/OUT9
{0x019E, 0xBC},{0x019F, 0x00},{0x01A0, 0x00},{0x01A1, 0x00},{0x01A2, 0x00},

// VCO setting, divider = 3
// 1500 MHz / 3 = 500 MHz
{0x01E0, 0x01},{0x01E1, 0x02},
// System
{0x0230, 0x00}
};

#define SPI_REG_RX0     0
#define SPI_REG_TX0     0
#define SPI_REG_RX1     4
#define SPI_REG_TX1     4
#define SPI_REG_RX2     8
#define SPI_REG_TX2     8
#define SPI_REG_RX3     12
#define SPI_REG_TX3     12

#define SPI_REG_CTRL     16
#define SPI_REG_DIVIDER     20
#define SPI_REG_SS     24

#define SPI_CTRL_ASS          (1<<13)
#define SPI_CTRL_IE          (1<<12)
#define SPI_CTRL_LSB          (1<<11)
#define SPI_CTRL_TXNEG          (1<<10)
#define SPI_CTRL_RXNEG          (1<<9)
#define SPI_CTRL_GO_BSY          (1<<8)
#define SPI_CTRL_CHAR_LEN(x)     ((x) & 0x7f)

#define GPIO_PLL_RESET_N 1
#define GPIO_SYS_CLK_SEL 0
#define GPIO_PERIPH_RESET_N 3

#define CS_PLL     0 /* AD9516 on SPI CS0 */

static void *oc_spi_base;

static int oc_spi_init(void *base_addr)
{
    oc_spi_base = base_addr;
    writel(100, oc_spi_base + SPI_REG_DIVIDER);
    return 0;
}

static int oc_spi_txrx(int ss, int nbits, uint32_t in, uint32_t *out)
{
    uint32_t rval;

    if (!out)
        out = &rval;

    writel(SPI_CTRL_ASS | SPI_CTRL_CHAR_LEN(nbits)
            | SPI_CTRL_TXNEG,
            oc_spi_base + SPI_REG_CTRL);

    writel(in, oc_spi_base + SPI_REG_TX0);
    writel((1 << ss), oc_spi_base + SPI_REG_SS);
    writel(SPI_CTRL_ASS | SPI_CTRL_CHAR_LEN(nbits)
            | SPI_CTRL_TXNEG | SPI_CTRL_GO_BSY,
            oc_spi_base + SPI_REG_CTRL);

    while(readl(oc_spi_base + SPI_REG_CTRL) & SPI_CTRL_GO_BSY)
        ;
    *out = readl(oc_spi_base + SPI_REG_RX0);
    return 0;
}

/*
 * AD9516 stuff, using SPI, used by later code.
 * "reg" is 12 bits, "val" is 8 bits, but both are better used as int
 */

static void ad9516_write_reg(int reg, int val)
{
    oc_spi_txrx(CS_PLL, 24, (reg << 8) | val, NULL);
}

static int ad9516_read_reg(int reg)
{
    uint32_t rval;
    oc_spi_txrx(CS_PLL, 24, (reg << 8) | (1 << 23), &rval);
    return rval & 0xff;
}

static void ad9516_update_regs(void)
{
    ad9516_write_reg(0x232, 0x1);
}

/* Sets the VCO divider (2..6) or 0 to enable static output */
static int ad9516_set_vco_divider(int ratio) 
{
    if(ratio == 0)
        ad9516_write_reg(0x1e0, 0x5); /* static mode */
    else
        ad9516_write_reg(0x1e0, (ratio-2));
    return 0;
}

static void ad9516_load_regset(const struct ad9516_reg *regs, int n_regs, int commit)
{
    int i;
    for(i=0; i<n_regs; i++)
        ad9516_write_reg(regs[i].reg, regs[i].val);
        
    if(commit)
        ad9516_write_reg(0x232, 1);
}

static void ad9516_wait_lock(void)
{
    while ((ad9516_read_reg(0x1f) & 1) == 0);
    pp_printf("AD9516 locked.\n");
}

#define SECONDARY_DIVIDER 0x100

static int ad9516_set_output_divider(int output, int ratio, int phase_offset)
{
    uint8_t lcycles = (ratio/2) - 1;
    uint8_t hcycles = (ratio - (ratio / 2)) - 1;
    int secondary = (output & SECONDARY_DIVIDER) ? 1 : 0;
    output &= 0xf;

    if(output >= 0 && output < 6) /* LVPECL outputs */
    {
        uint16_t base = (output / 2) * 0x3 + 0x190;

        if(ratio == 1)  /* bypass the divider */
        {
            uint8_t div_ctl = ad9516_read_reg(base + 1);
            ad9516_write_reg(base + 1, div_ctl | (1<<7) | (phase_offset & 0xf)); 
        } else {
            uint8_t div_ctl = ad9516_read_reg(base + 1);
            ad9516_write_reg(base + 1, (div_ctl & (~(1<<7))) | (phase_offset & 0xf));  /* disable bypass bit */
            ad9516_write_reg(base, (lcycles << 4) | hcycles);
        }
    } else { /* LVDS/CMOS outputs */
            
        uint16_t base = ((output - 6) / 2) * 0x5 + 0x199;

        // pp_printf("Output [divider %d]: %d ratio: %d base %x lc %d hc %d\n", secondary, output, ratio, base, lcycles ,hcycles);

        if(!secondary)
        {
            if(ratio == 1)  /* bypass the divider 1 */
                ad9516_write_reg(base + 3, ad9516_read_reg(base + 3) | 0x10); 
            else {
                ad9516_write_reg(base, (lcycles << 4) | hcycles); 
                ad9516_write_reg(base + 1, phase_offset & 0xf);
            }
        } else {
            if(ratio == 1)  /* bypass the divider 2 */
                ad9516_write_reg(base + 3, ad9516_read_reg(base + 3) | 0x20); 
            else {
                ad9516_write_reg(base + 2, (lcycles << 4) | hcycles); 
//                    ad9516_write_reg(base + 1, phase_offset & 0xf);
                
            }
        }          
    }

    /* update */
    ad9516_write_reg(0x232, 0x1);
    return 0;
}

static void ad9516_sync_outputs(void)
{
    /* Sync the outputs when they're inactive to avoid +-1 cycle uncertainity */
    ad9516_write_reg(0x230, 1);
    ad9516_write_reg(0x232, 1);
    ad9516_write_reg(0x230, 0);
    ad9516_write_reg(0x232, 1);
}

int pll_ad9516_init(unsigned char *BASE_SPI)
{
    pp_printf("Initializing AD9516 PLL...\n");

    oc_spi_init((void *)BASE_SPI);

    /* Use unidirectional SPI mode */
    ad9516_write_reg(0x000, 0x99);

     /* Check the presence of the chip */
    if (ad9516_read_reg(0x3) != 0xc3) {
        pp_printf("Error: AD9516 PLL not responding.\n");
        return -1;
    }

    ad9516_load_regset(ad9516_ext_base_config, ARRAY_SIZE(ad9516_ext_base_config), 1);    
    ad9516_sync_outputs();
    // ad9516_wait_lock();
    // ad9516_sync_outputs();
    ad9516_set_vco_divider(3); 

    return 0;
}