/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __GPIO_H
#define __GPIO_H

#include <stdint.h>

#include "board.h"

#define GPIO_SYS_CLK_SEL	0
#define GPIO_PLL_RESET_N	1
#define GPIO_PERIPH_RESET_N	3
#define GPIO_LJD_BOARD_DETECT	4
#define GPIO_LJD_OSC_FREQ_0	5
#define GPIO_LJD_OSC_FREQ_1	6
#define GPIO_LJD_OSC_FREQ_2	7

extern int ljd_present;

struct GPIO_WB
{
  uint32_t CODR;  /*Clear output register*/
  uint32_t SODR;  /*Set output register*/
  uint32_t DDR;   /*Data direction register (1 means out)*/
  uint32_t PSR;   /*Pin state register*/
};

static volatile struct GPIO_WB *__gpio = (volatile struct GPIO_WB *) BASE_GPIO;

static inline void gpio_out(int pin, int val)
{
  if(val)
    __gpio->SODR = (1<<pin);
  else
    __gpio->CODR = (1<<pin);
}

static inline void gpio_dir(int pin, int val)
{
  if(val)
    __gpio->DDR |= (1<<pin);
  else
    __gpio->DDR &= ~(1<<pin);
}

static inline int gpio_in(int pin)
{
  return __gpio->PSR & (1<<pin) ? 1: 0;
}

        
#endif
        
