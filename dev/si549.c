/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Quentin Genoud Duvillaret <quentin.genoud@cern.ch>
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

#include "dev/si549.h"

#include "hw/si549_if_wb.h"

#include <wrc-debug.h>
#include <hw/rawmem.h>
#include "dev/syscon.h"

//#define FULL_CAST

#ifdef FULL_CAST
/* Macros to divide integers to 43bit fixed point (11.32) for Si549 FBDIV */
#define FIXP_DIV_INT(val, div)		(((uint64_t)(((uint64_t)val) / ((uint64_t)div))) & ((uint64_t)0x7FFUL))
#define FIXP_DIV_FRAC(val, div)		((uint64_t)(((((uint64_t)val) % ((uint64_t)div)) * ((uint64_t)0x100000000UL)) / ((uint64_t)div)) & ((uint64_t)0xFFFFFFFFUL))
#define FIXP_DIV(val, div)			((FIXP_DIV_INT(val, div) << ((uint64_t)32UL)) | FIXP_DIV_FRAC(val, div))
#else
/* Macros to divide integers to 43bit fixed point (11.32) for Si549 FBDIV */
#define FIXP_DIV_INT(val, div)		((val / div) & 0x7FFUL)
#define FIXP_DIV_FRAC(val, div)		((((val % div) * 0x100000000UL) / div) & 0xFFFFFFFFUL)
#define FIXP_DIV(val, div)			((FIXP_DIV_INT(val, div) << 32UL) | FIXP_DIV_FRAC(val, div))
#endif

/* Scale is 10^8 to display eight 0s after decimal dot */
#define FIXP_FRAC2INT(frac)			((((uint64_t)frac & 0xFFFFFFFFUL) * 100000000UL) / 0x100000000UL)
/* Add half the divisor (2^31) for rounding */
#define FIXP_FRAC2INT_RND(frac)		((((uint64_t)frac & 0xFFFFFFFFUL) * 100000000UL + (0x100000000UL >> 1UL)) / 0x100000000UL)

#define SI549_PIN_SCL 0
#define SI549_PIN_SDA 1

#define GAIN_FIXP3_13	0x53E5

void si549_gpio_out(const struct gpio_pin *pin, int value)
{
	struct wr_si549_interface_device* dev = ( struct wr_si549_interface_device* ) pin->device->priv;



	uint32_t mask = (pin->pin == SI549_PIN_SCL ? SI549_IF_WB_GPCR_SCL : SI549_IF_WB_GPCR_SDA);
	uint32_t reg = (value ? SI549_IF_WB_GPSR : SI549_IF_WB_GPCR );


	writel( mask, dev->base_addr + reg );
}


void si549_gpio_set_dir(const struct gpio_pin *pin, int dir)
{
	si549_gpio_out(pin, !dir);
}


int si549_gpio_in(const struct gpio_pin *pin)
{
	struct wr_si549_interface_device* dev = ( struct wr_si549_interface_device* ) pin->device->priv;

	uint32_t gpsr = readl(dev->base_addr + SI549_IF_WB_GPSR);

	if ( pin->pin == SI549_PIN_SCL )
		return (gpsr & SI549_IF_WB_GPSR_SCL ? 1 : 0);
	else
		return (gpsr & SI549_IF_WB_GPSR_SDA ? 1 : 0);
}


void si549_read(struct wr_si549_interface_device *dev, uint8_t addr, uint8_t *data, int count)
{
	int i;

	bb_i2c_start( &dev->master );
	bb_i2c_put_byte( &dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( &dev->master, addr );
	bb_i2c_repeat_start( &dev->master );
	bb_i2c_put_byte( &dev->master, (dev->i2c_addr << 1) | 1 );

	for(i = 0; i < count; i ++)
		bb_i2c_get_byte( &dev->master, &data[i], i == (count - 1) ? 1 : 0 );

	bb_i2c_stop( &dev->master );
}


void si549_write(struct wr_si549_interface_device *dev, uint8_t addr, uint8_t *data, int count)
{
	int i;

	bb_i2c_start( &dev->master );
	bb_i2c_put_byte( &dev->master, dev->i2c_addr << 1 );
	bb_i2c_put_byte( &dev->master, addr );

	for(i = 0; i < count; i ++)
	{
		bb_i2c_put_byte( &dev->master, data[i] );
	}

	bb_i2c_stop( &dev->master );
}


void si549_reset(struct wr_si549_interface_device *dev)
{
	uint8_t r7;

	/* Register 7, bit 7: Reset */
	r7 = (1<<7);
	si549_write(dev, 7, &r7, 1);

	/* Wait power-up time */
	timer_delay_ms(10);
}

uint8_t si549_get_id(struct wr_si549_interface_device *dev)
{
	uint8_t id;

	si549_read(dev, 0x00, &id, 1);

	return id;
}


void si549_get_frequency(struct wr_si549_interface_device *dev, uint32_t* freq_hz)
{
	/* Si549 Fosc = 152.6MHz */
	const uint64_t f_osc = 152600000UL;
	/* FBDIV fractional part width is 32bit, divide by 2^32 */
	const uint64_t frac_div = 0x100000000UL;
	uint64_t f_out;
	uint8_t lsdiv_reg;
	uint8_t lsdiv_val;
	uint16_t hsdiv;
	uint16_t fbdiv_int;
	uint32_t fbdiv_frac;
	uint8_t regs[10];
	
	/* Read registers 23 to 31 */
	si549_read(dev, 23, regs, 9);

	/* LSDIV: reg[24](6 downto 4) */
	lsdiv_reg = (regs[1] >> 4) & 0x07;
	/* Get the real value of lsdiv */
	lsdiv_val = (lsdiv_reg <= 5) ? (1 << lsdiv_reg) : 32;

	/* HSDIV: reg[24](2 downto 0) & reg[23](7 downto 0)*/
	hsdiv = ( ((regs[1] & 0x07) << 8) | regs[0] ) & 0x7FF;

	/* FBDIV = reg[31](2 downto 0) & reg[30] & reg[29] & reg[28] & reg[27] & reg[26]
	 * FBDIV_INT (11bit integer value): FBDIV[42:32] = reg[31](2 downto 0) & reg[30] 
	 * FBDIV_FRAC (32bit fractional value): FBDIV[31:0] = reg[29] & reg[28] & reg[27] & reg[26]
	 * Print FBDIV value: "FBDIV_INT.(FBDIV_FRAC*scale+2^31)/2^32*/
	fbdiv_int = ( ((regs[8] & 0x07) << 8) | regs[7] ) & 0x7FF;
	fbdiv_frac = ( regs[6] << 24 ) |
				 ( regs[5] << 16 ) |
				 ( regs[4] << 8 )  |
				 ( regs[3] << 0 );

	/* Fout = (Fosc * FBDIV) / (HSDIV * LSDIV)
	 * Integer part */
	f_out = (f_osc * fbdiv_int) / (hsdiv * lsdiv_val);

	/*board_dbg("Si549: fosc = %X%XHz, fbdiv_int = %u, hsdiv = %u, lsdiv_val = %u\n", (uint32_t)((f_osc >> 32) & 0xFFFFFFFF), (uint32_t)(f_osc & 0xFFFFFFFF), fbdiv_int, hsdiv, lsdiv_val);
	board_dbg("Si549: fosc = %luHz ", f_osc);
	board_dbg("fbdiv_int = %u, ", fbdiv_int);
	board_dbg("hsdiv = %u, ", hsdiv);
	board_dbg("lsdiv_val = %u\n", lsdiv_val);*/

	/* Add rounded fractionnal part */
	f_out += (f_osc * fbdiv_frac + (frac_div >> 1)) / (hsdiv * lsdiv_val * frac_div);

	board_dbg("Si549: get freq.\n - LSDIV = %u (/%u)\n - HSDIV = %u\n - FBDIV = %u.%u\n - Calc. Freq. = %u Hz\n",
		lsdiv_reg, lsdiv_val, hsdiv, fbdiv_int, (uint32_t)FIXP_FRAC2INT(fbdiv_frac), (uint32_t)f_out);

	if( freq_hz )
		*freq_hz = (uint32_t)f_out;
}


int si549_calc_frequency(uint32_t freq_hz, uint8_t *lsdiv, uint16_t *hsdiv, uint64_t *fbdiv)
{
	
	const uint64_t f_osc = 152600000;
	const uint64_t f_vco_min = 10800000000;
	const uint64_t f_vco_max = 12206718160;
	const uint16_t hsdiv_min = 5;
	const uint16_t hsdiv_max = 2046;
	uint8_t lsdiv_shift;
	uint8_t lsdiv_val;
	uint64_t f_vco;
	uint64_t hsdiv_i, hsdiv_f;
	uint64_t tmp_i, tmp_f;
	
	for(lsdiv_shift = 0UL; lsdiv_shift <= 5UL; lsdiv_shift++)
	{
		/* Get LSDIV */
		lsdiv_val = 1UL << lsdiv_shift;

		/* HSDIV * LSDIV = Fvco_min / Fout */
		tmp_i = FIXP_DIV_INT(f_vco_min, freq_hz);
		tmp_f = FIXP_DIV_FRAC(f_vco_min, freq_hz);

		/* Try to get a value for hsdiv */
		hsdiv_i = FIXP_DIV_INT(tmp_i, lsdiv_val);
		/* Divide fractional part tmp_f as it was an integer and add fractional part of tmp_i division */
		hsdiv_f = FIXP_DIV_INT(tmp_f, lsdiv_val) + FIXP_DIV_FRAC(tmp_i, lsdiv_val);

		/* Always round hsdiv to superior integer */
		if(hsdiv_f > 0UL)
			hsdiv_i++;

		/* hsdiv must be even if it is greater than 33 */
		if( (hsdiv_i > 33UL) && (hsdiv_i % 2UL) )
			hsdiv_i++;

		if(hsdiv_i < (uint64_t)hsdiv_min || hsdiv_i > (uint64_t)hsdiv_max)
			continue;

		/* If we are here, then we have valid LSDIV and HSDIV values */
		/* Calculate VCO frequency */
		f_vco = (uint64_t)(((uint64_t)lsdiv_val) * ((uint64_t)hsdiv_i) * ((uint64_t)freq_hz));
	
		if((f_vco < f_vco_min) || (f_vco > f_vco_max))
		{
			board_dbg("fvco (=0x%X%X) < min (=0x%X%X) or > max (=0x%X%X)\n", 
				(uint32_t)(f_vco >> 32), (uint32_t)f_vco,
				(uint32_t)(f_vco_min >> 32), (uint32_t)f_vco_min,
				(uint32_t)(f_vco_max >> 32), (uint32_t)f_vco_max);
			continue; // Is that right to loop here ???
		}

		/* Check pointers before saving values */
		if(!lsdiv || !hsdiv || !fbdiv)
			return -1;

		/* LSDIV value to be written in register is shift value */
		*lsdiv = lsdiv_shift;

		/* Save hsdiv register value */
		*hsdiv = (uint16_t)(hsdiv_i);

		/* Compute 43bit fbdiv value to be written in registers */
		*fbdiv = FIXP_DIV(f_vco, f_osc);

		board_dbg("Si549: calc. freq.\n - Req. freq = %uHz\n - LSDIV = %u (/%u)\n - HSDIV = %u\n - FBDIV = 0x%X%X (=%u.%u)\n - Calc. freq = %lu\n",
			freq_hz, *lsdiv, lsdiv_val, *hsdiv, (uint32_t)(*fbdiv >> 32), (uint32_t)*fbdiv,
			(uint32_t)(*fbdiv >> 32), (uint32_t)FIXP_FRAC2INT(*fbdiv), (uint32_t)(f_vco / (hsdiv_i * lsdiv_val)));

		return 0;
	}

	/* If we are here, it means no values have been found to generate the desired frequency */
	return -1;
}



#if 0
int si549_calc_frequency(uint32_t freq_hz, uint8_t *lsdiv, uint16_t *hsdiv, uint64_t *fbdiv)
{
	
	const uint64_t f_osc = 152600000;
	const uint64_t f_vco_min = 10800000000;
	const uint64_t f_vco_max = 12206718160;
	const uint16_t hsdiv_min = 5;
	const uint16_t hsdiv_max = 2046;
	uint8_t lsdiv_shift;
	uint8_t lsdiv_val;
	uint64_t f_vco;
	uint64_t hsdiv_i, hsdiv_f;
	uint64_t tmp_i, tmp_f;
	
	//board_dbg("LOOP START 0x%X, 0x%X, 0x%x\n", (uint32_t)(&f_osc), (uint32_t)(&tmp_f), (uint32_t)(&lsdiv_shift));
	//board_dbg("LOOP START 0x%X\n", (uint32_t)(&tmp_f));
	//board_dbg("LOOP START lsdiv %p hsdiv %p fbdiv %p\n", lsdiv, hsdiv, fbdiv);
	//disable_irq();
	for(lsdiv_shift = 0UL; lsdiv_shift <= 5UL; lsdiv_shift++)
	{
		/* Get LSDIV */
		lsdiv_val = 1UL << lsdiv_shift;

		/* HSDIV * LSDIV = Fvco_min / Fout */
		tmp_i = FIXP_DIV_INT(f_vco_min, freq_hz);
		//pp_printf("step1\n");
		tmp_f = FIXP_DIV_FRAC(f_vco_min, freq_hz);
		//pp_printf("step2\n");
		/* Try to get a value for hsdiv */
		hsdiv_i = FIXP_DIV_INT(tmp_i, lsdiv_val);
		//pp_printf("step3 lsdiv_val %d\n", (uint32_t)lsdiv_val );
		hsdiv_f = FIXP_DIV_INT(tmp_f, lsdiv_val);
		hsdiv_f += FIXP_DIV_FRAC(tmp_i, lsdiv_val);
		//pp_printf("step4\n");
		
		//pp_printf("tmp_i %d f %d hsdiv i %d f %d\n\r", tmp_i, tmp_f, hsdiv_i, hsdiv_f );
		//board_dbg("1)\n");
		/* Always round hsdiv to superior integer */
		if(hsdiv_f > 0UL)
		{
			hsdiv_i++;
			//hsdiv_f = 0UL;
		}
		//board_dbg("2)\n");
	#if 1
		/* hsdiv must be even if it is greater than 33 */
		if( (hsdiv_i > 33UL) && (hsdiv_i % 2UL) )
			hsdiv_i++;

		if(hsdiv_i < (uint64_t)hsdiv_min || hsdiv_i > (uint64_t)hsdiv_max)
			continue;
	#endif
	#if 0
		/* If we are here, then we have valid LSDIV and HSDIV values */
		/* Calculate VCO frequency */
		//board_dbg("lsdiv_val = %X, hsdiv_i = 0x%X%X, freq_hz = 0x%X\n", lsdiv_val, (uint32_t)(hsdiv_i>>32), (uint32_t)hsdiv_i, freq_hz);
		f_vco = (uint64_t)(((uint64_t)lsdiv_val) * ((uint64_t)hsdiv_i) * ((uint64_t)freq_hz));
	
		if((f_vco < f_vco_min) || (f_vco > f_vco_max))
		{
			board_dbg("fvco (=0x%X%X) < min (=0x%X%X) or > max (=0x%X%X)\n", 
			(uint32_t)(f_vco >> 32), (uint32_t)f_vco,
			(uint32_t)(f_vco_min >> 32), (uint32_t)f_vco_min,
			(uint32_t)(f_vco_max >> 32), (uint32_t)f_vco_max);
			continue; // Is that right to loop here ???
		}

		/* Check pointers before saving values */
		if(!lsdiv || !hsdiv || !fbdiv)
			return -1;
#if 0
		/* LSDIV value to be written in register is shift value */
		*lsdiv = lsdiv_shift;

		/* Save hsdiv register value */
		*hsdiv = (uint16_t)(hsdiv_i);

		/* Compute 43bit fbdiv value to be written in registers */
		*fbdiv = (uint32_t)FIXP_DIV(f_vco, f_osc);
#endif
	#endif
	#if 0
		board_dbg("Si549:\n - Req. freq = %uHz\n - LSDIV = %u (/%u)\n - HSDIV = %u\n - FBDIV = 0x%lX (=%lu.%lu)\n - Calc. freq = %lu\n",
			freq_hz, *lsdiv, lsdiv_val, *hsdiv, *fbdiv, *fbdiv >> 32,
			FIXP_FRAC2INT(*fbdiv), f_vco / (hsdiv_i * lsdiv_val));
	#endif
	#if 1
		board_dbg("LOOP END\n");
		//return 0; // WTF??? causing modifications on bin file @ 0x00
	#endif
	}

	/* If we are here, it means no values have been found to generate the desired frequency */
	return -1;
}
#endif

int si549_set_frequency(struct wr_si549_interface_device *dev, uint32_t freq_hz, int vco_gain)
{
	uint8_t regs[10];
	uint8_t reg;
	uint8_t lsdiv;
	uint16_t hsdiv;
	uint64_t fbdiv;

	/* Compute register values to match requested frequency */
	if(si549_calc_frequency(freq_hz, &lsdiv, &hsdiv, &fbdiv) < 0)
		return -1;

	/* r7: Reset */
	reg = 0x80;
	si549_write(dev, 7, &reg, 1);
	
	/* r255: Set page register to point to page 0 */
	reg = 0x00;
	si549_write(dev, 255, &reg, 1);

	/* r69: Disable FCAL override (to allow FCAL for this Freq Update) */
	reg = 0x00;
	si549_write(dev, 69, &reg, 1);

	/* r17: Synchronously disable output  */
	reg = 0x00;
	si549_write(dev, 17, &reg, 1);

	/* LSDIV: reg[24](6 downto 4)*/
	regs[1] = (lsdiv & 0x07) << 4;

	/* HSDIV: reg[24](2 downto 0) & reg[23](7 downto 0) */
	regs[0] = hsdiv & 0xFF;
	regs[1] |= (hsdiv >> 7) & 0x07;

	/* FBDIV = reg[31](2 downto 0) & reg[30] & reg[29] & reg[28] & reg[27] & reg[26]
	 * FBDIV_INT (11bit integer value): FBDIV[42:32] = reg[31](2 downto 0) & reg[30] 
	 * FBDIV_FRAC (32bit fractional value): FBDIV[31:0] = reg[29] & reg[28] & reg[27] & reg[26] */
	regs[3] = fbdiv & 0xFF;
	regs[4] = (fbdiv >> 8) & 0xFF;
	regs[5] = (fbdiv >> 16) & 0xFF;
	regs[6] = (fbdiv >> 24) & 0xFF;
	regs[7] = (fbdiv >> 32) & 0xFF;
	regs[8] = (fbdiv >> 40) & 0x07;

	/* Write LSDIV, HSDIV & FBDIV: r23-r24, then r26-r31 */
	si549_write(dev, 23, &regs[0], 2);
	si549_write(dev, 26, &regs[3], 6);

	/* r7: Start FCAL */
	reg = (1 << 3);
	si549_write(dev, 7, &reg, 1);

	/* r17: Synchronously enable output */
	reg = 0x01;
	si549_write(dev, 17, &reg, 1);

	/* Wait max settling time (40 ms) */
	timer_delay_ms(40);

	board_dbg("Si549: VCO Gain = %d\n", vco_gain);

	/* Set the gain to adapt command from WRSv3 to the new VCXO of WRSv4 */
	writel((GAIN_FIXP3_13 << SI549_IF_WB_GAIN_GAIN_VALUE_SHIFT) & SI549_IF_WB_GAIN_GAIN_VALUE_MASK,
		dev->base_addr + SI549_IF_WB_GAIN);

	/* Set I2C address, enable SPLL FSM, set gain and set I2C freq prescaler in CR */
	writel( (((dev->i2c_addr << 1) << SI549_IF_WB_CR_I2C_ADDR_SHIFT) & SI549_IF_WB_CR_I2C_ADDR_MASK) |
			SI549_IF_WB_CR_ENABLE 																	 | 
			((vco_gain << SI549_IF_WB_CR_GAIN_SHIFT) & SI549_IF_WB_CR_GAIN_MASK)					 |
		   	((200 << SI549_IF_WB_CR_CLK_DIV_SHIFT) & SI549_IF_WB_CR_CLK_DIV_MASK),
			dev->base_addr + SI549_IF_WB_CR);

	return 0;
}


void wr_si549_interface_init(struct wr_si549_interface_device *dev, uint32_t base_addr, uint8_t i2c_addr)
{
	dev->base_addr = (void *) base_addr;
	dev->gpio_i2c.priv = (void *) dev;
	dev->gpio_i2c.read_pin = si549_gpio_in;
	dev->gpio_i2c.set_dir = si549_gpio_set_dir;
	dev->gpio_i2c.set_out = si549_gpio_out;
	dev->i2c_addr = i2c_addr;
	dev->pin_scl.device = &dev->gpio_i2c;
	dev->pin_scl.pin = SI549_PIN_SCL;
	dev->pin_sda.device = &dev->gpio_i2c;
	dev->pin_sda.pin = SI549_PIN_SDA;
	bb_i2c_create( &dev->master, &dev->pin_scl, &dev->pin_sda );
	bb_i2c_scan( &dev->master );
}
