#include <stdint.h>
#include <stdio.h>

#include "ltc6950.h"

// Read from LTC6950 via SPI
uint8_t ltc6950_read(struct ltc6950_device *dev, uint32_t reg) {
    uint8_t rv;
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1) | 1, 8);
    rv = bb_spi_read(dev->bus, 8);
    bb_spi_cs(dev->bus, 0);
    return rv;
}

void ltc6950_write(struct ltc6950_device *dev, uint32_t reg, uint8_t value) {
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, (reg << 1), 8);
    bb_spi_write(dev->bus, value, 8);
    bb_spi_cs(dev->bus, 0);
};

#define LTC6950_R0_LOCK (1<<2)

int ltc6950_configure(struct ltc6950_device *dev, struct ltc6950_config* cfg)
{
    int i;

    for(i = 0; i < cfg->n_regs; i++) {
      //  pp_printf("LTC write %x %x\n", cfg->regs[i].addr, cfg->regs[i].value);
        ltc6950_write(dev, cfg->regs[i].addr, cfg->regs[i].value);
    }

    //for(;;)
    {
        uint8_t r0 = ltc6950_read( dev, 0 );
        //pp_printf("tlc r0 = %x\n", r0 );
        timer_delay_ms(100);
    }
}