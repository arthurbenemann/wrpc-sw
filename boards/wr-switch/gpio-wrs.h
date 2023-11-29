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
extern const struct gpio_pin gpio_pin_ljd_osc_freq_0;
extern const struct gpio_pin gpio_pin_ljd_osc_freq_1;
extern const struct gpio_pin gpio_pin_ljd_osc_freq_2;
extern const struct gpio_pin gpio_pin_ljd_periph_id_0;
extern const struct gpio_pin gpio_pin_ljd_periph_id_1;
extern const struct gpio_pin gpio_pin_ljd_periph_id_2;

void wrs_gpio_init(void);

#endif
