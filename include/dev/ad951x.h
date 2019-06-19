#ifndef __AD951x_H
#define __AD951x_H

#include <stdint.h>

#include "board.h"
#include "dev/gpio.h"
#include "dev/spi.h"

#define AD951X_BIT_DELAY 100

struct ad951x_config_reg {
    uint16_t addr;
    uint8_t value;
};

struct ad951x_config {
    int n_regs;
    struct ad951x_config_reg regs[];
};

struct ad951x_device {
    struct spi_bus *bus;
    struct gpio_pin *pin_reset;
    struct gpio_pin *pin_lock;
};


void ad951x_write(struct ad951x_device *dev, uint32_t reg, uint32_t value);
uint32_t ad951x_read(struct ad951x_device *dev, uint32_t reg);
int ad951x_configure(struct ad951x_device *dev, struct ad951x_config *cfg);
int ad951x_init(struct ad951x_device *dev, struct spi_bus *spi, struct gpio_pin *pin_reset, struct gpio_pin *pin_lock);

#endif
