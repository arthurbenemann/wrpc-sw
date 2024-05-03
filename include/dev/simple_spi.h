/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __SIMPLE_SPI_H
#define __SIMPLE_SPI_H

#include <stdint.h>

#define SSPI_POS_EDGE   0
#define SSPI_NEG_EDGE   1

#define SSPI_MAN_SS     0
#define SSPI_AUTO_SS    1

struct simple_spi_device {
  void* base;
  uint32_t edge;
  uint32_t anss;
};

int sspi_init(struct simple_spi_device *dev, uint32_t base_addr, uint32_t freq, uint8_t edge, uint8_t anss);
uint32_t sspi_transfer(struct simple_spi_device *dev, uint8_t bits, uint32_t data);

#if 0
void sspi_select(struct simple_spi_device *dev, uint8_t slave_nb);
void sspi_unselect(struct simple_spi_device *dev, uint8_t slave_nb);
void sspi_xfer_bytes(struct simple_spi_device *dev, uint8_t slave_nb, uint8_t *tx_data, uint8_t *rx_data, uint32_t len);
#endif


#endif
