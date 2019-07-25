/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __SPI_FLASH_H
#define __SPI_FLASH_H

#include "dev/spi.h"

struct spi_flash_device 
{
    struct spi_bus *bus;
    uint32_t sector_size;
};


void spi_flash_create(struct spi_flash_device *dev, struct spi_bus *bus);
int spi_flash_write(struct spi_flash_device *dev, uint32_t addr, uint8_t *buf, int count);
int spi_flash_read(struct spi_flash_device *dev, uint32_t addr, uint8_t *buf, int count);
uint32_t spi_flash_read_id(struct spi_flash_device *dev);
void spi_flash_erase_sector(struct spi_flash_device *dev, uint32_t addr);
int spi_flash_erase(struct spi_flash_device *dev, uint32_t addr, int count);


#endif