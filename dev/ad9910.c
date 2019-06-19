
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

void ad9910_write(struct ad9910_device *dev, uint32_t reg, uint8_t value)
{

}

int ad9910_probe( struct ad9910_device *dev, struct spi_bus *bus )
{

    
    dev->bus = bus;
    bb_spi_cs(dev->bus, 0);
    
    for(;;)
    {
        uint32_t id = ad9910_read( dev, 1, 32 );
        pp_printf("AD9910 ID[%p]: 0x%x\n", dev, id );
    }

    return 0;
}
