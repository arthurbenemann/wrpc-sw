#ifndef __AD9510_H
#define __AD9510_H

#include "board.h"
#include "gpio.h"
#include "spi.h"

#define BIT_DELAY 100


struct ad95xx_config_reg {
    uint16_t addr;
    uint8_t value;
};

struct ad95xx_config {
    int n_regs;
    struct ad95xx_config_reg regs[];
};


void ad9510_write(struct spi_bus *bus, uint32_t reg, uint32_t value);
uint32_t ad9510_read(struct spi_bus *bus, uint32_t reg);
int ad9510_configure(struct spi_bus *bus, struct ad95xx_config *cfg);
void ad9510_init();

#endif
