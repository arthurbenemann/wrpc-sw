/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __AUXCLK_H
#define __AUXCLK_H

#include <stdint.h>

int auxclk_init(uint32_t freq, uint32_t duty);
int auxclk_get_settings(uint32_t *freq, uint32_t *duty);

#endif