#ifndef __AD9910_H
#define __AD9910_H


#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"

struct ad9910_device {
    struct spi_bus *bus;
    struct gpio_pin *pin_ioupdate;
};

struct ad9910_config_reg {
    int addr;
    uint64_t value;
    int nbits;
};

int ad9910_program( struct ad9910_device *dev, uint64_t freq_hz, int phase, int fs_current );
uint64_t ad9910_read(struct ad9910_device *dev, uint32_t reg, int nbits);
void ad9910_write(struct ad9910_device *dev, uint32_t reg, uint64_t value, int nbits);
int ad9910_probe( struct ad9910_device *dev, struct spi_bus *bus );
void ad9910_trigger_update(struct ad9910_device *dev);

#endif
