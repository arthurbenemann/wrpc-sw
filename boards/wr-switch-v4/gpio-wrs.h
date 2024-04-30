/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __GPIO_WRS_H
#define __GPIO_WRS_H

#include "dev/gpio.h"

extern const struct gpio_pin gpio_pin_sys_clk_sel;
extern const struct gpio_pin gpio_pin_pll_reset_n;
extern const struct gpio_pin gpio_pin_periph_reset_n;
extern const struct gpio_pin gpio_pin_ljd_board_detect;
extern const struct gpio_pin gpio_pin_ep_reset_n;

extern const struct gpio_pin gpio_pin_pll_clk_sel;
extern const struct gpio_pin gpio_pin_pll_sync;
extern const struct gpio_pin gpio_pin_pll_gpio1;
extern const struct gpio_pin gpio_pin_pll_gpio2;

void wrs_gpio_init(void);

#endif