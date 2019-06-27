
#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"
#include "dev/ad9910.h"

uint32_t ad9910_read(struct ad9910_device *dev, uint32_t reg, int nbits)
{
    uint32_t rv;
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, reg | 0x80, 8);
    rv = bb_spi_read(dev->bus, nbits);
    bb_spi_cs(dev->bus, 0);
    return rv;
}

void ad9910_write(struct ad9910_device *dev, uint32_t reg, uint32_t value)
{
    bb_spi_cs(dev->bus, 1);
    bb_spi_write( dev->bus, reg, 8);
    bb_spi_write( dev->bus, value, 32);
    bb_spi_cs(dev->bus, 0);
}

void ad9910_trigger_update(struct ad9910_device *dev)
{
    gen_gpio_out(dev->pin_ioupdate, 1); // acknowledge
    gen_gpio_out(dev->pin_ioupdate, 0); 
}

#define AD9910_REG_CFR1 1
#define AD9910_DEFAULT_CFR1  0x400820

int ad9910_probe( struct ad9910_device *dev, struct spi_bus *bus )
{
    dev->bus = bus;
    bb_spi_cs(dev->bus, 0);

    gen_gpio_out(dev->pin_ioupdate, 0);

    ad9910_write( dev, 0, 0x02000002); // unidir mode for SDIO
    ad9910_trigger_update( dev );

    uint32_t id = ad9910_read( dev, AD9910_REG_CFR1, 32 );
    pp_printf("AD9910 ID[%p]: 0x%x (expected 0x%x)\n", dev, id, AD9910_DEFAULT_CFR1 );

    return (id == AD9910_DEFAULT_CFR1) ? 0 : -1;
}
