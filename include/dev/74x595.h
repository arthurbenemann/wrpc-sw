#ifndef __SN74x595_H
#define __SN74x595_H

int x595_gpio_create(struct gpio_device *device, int n_regs, struct gpio_pin *pin_rclk, struct gpio_pin *pin_srclk, struct gpio_pin *pin_srclr_n, struct gpio_pin *pin_ser);

#endif
