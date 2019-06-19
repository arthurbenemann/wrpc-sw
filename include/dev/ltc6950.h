#ifndef __LTC6950_H
#define __LTC6950_H

#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"

struct ltc6950_device {
    struct spi_bus *bus;
};

struct ltc6950_config_reg {
    uint16_t addr;
    uint8_t value;
};

struct ltc6950_config {
    int n_regs;
    struct ltc6950_config_reg regs[];
};



uint8_t ltc6950_read(struct ltc6950_device *dev, uint32_t reg);
void ltc6950_write(struct ltc6950_device *dev, uint32_t reg, uint8_t value);
int ltc6950_configure(struct ltc6950_device *dev, struct ltc6950_config* cfg);

#endif
