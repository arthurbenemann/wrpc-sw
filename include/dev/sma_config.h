#ifndef __SMA_CONFIG_H
#define __SMA_CONFIG_H

#include <stdint.h>

int sma_sel(uint32_t port, uint32_t source);

int sma_customed_set(uint32_t port, uint32_t prh, uint32_t prl, uint32_t csr);

int sma_fdly_set(uint32_t port, uint32_t step);

void sma_init();

#endif
