#include "hw/rawmem.h"
#include "hw/wb_spi.h"
#include "dev/simple_spi.h"
#include "board.h"

#define SSPI_CALC_PRESCALER(freq) ((uint8_t)((2 * CPU_CLOCK) / freq))

/*
 * Initialize SPI peripheral
 * edge: 0 = set data on negative edge, sample on positive edge
 * edge: 1 = set data on positive edge, sample on negative edge
 * anss: 1 = automatic slave select
 */
int sspi_init(struct simple_spi_device *dev, uint32_t base_addr, uint32_t freq, uint8_t edge, uint8_t anss)
{
    if(!dev)
        return -1;

    dev->base = base_addr;

    /* If edge = 0: positive edge sampling */
    if(!edge)
        dev->edge = SPI_CTRL_TX_NEGEDGE | SPI_CTRL_RX_NEGEDGE;
    else
        dev->edge = 0;

    /* Automatic slave select */
    if(anss)
        dev->anss = SPI_CTRL_ASS;
    else
        dev->anss = 0;

    /* Set prescaler to 625: Fspi = 62500000 Hz / (2*100) = 625 kHz */
    writel(SSPI_CALC_PRESCALER(freq), dev->base + SPI_DIVIDER);
    
    /* Set slave number once here (only one slave) */
    writel(SPI_SS_VALUE, dev->base + SPI_SS);
}

/*
 * Start a transfer
 * nbits: number of bits to transmit in the frame
 * data: data to transmit
 * returns data received
 */
uint32_t sspi_transfer(struct simple_spi_device *dev, uint8_t nbits, uint32_t data)
{
    uint32_t ctrl;

    /* Write frame in Tx buffer */
    writel(data, dev->base + SPI_TX_RX_0);

    /* Build ctrl register to start transfer */
    ctrl = ((nbits & SPI_CTRL_LEN_MASK) << SPI_CTRL_LEN_SHIFT) |
        dev->edge   |
        dev->anss   |
        SPI_CTRL_GO;
    
    /* Start the transfer */
    writel(ctrl, dev->base + SPI_CTRL);
    
    /* Wait for transfer to complete */
    do{
        ctrl = readl(dev->base + SPI_CTRL);
    }while(ctrl & SPI_CTRL_GO);
    
    /* Return read value */
    return readl(dev->base + SPI_TX_RX_0);
}

#if 0
void sspi_select(struct simple_spi_device *dev, uint8_t slave_nb)
{
    slave_nb = (1 << (slave_nb - 1));
    writel(slave_nb, dev->base + SPI_SS);
}

void sspi_unselect(struct simple_spi_device *dev, uint8_t slave_nb)
{
    uint32_t ss = readl(dev->base + SPI_SS);
    ss &= ~(1 << (slave_nb - 1));
    writel(ss, dev->base + SPI_SS);
}

void sspi_xfer_bytes(struct simple_spi_device *dev, uint8_t slave_nb, uint8_t *tx_data, uint8_t *rx_data, uint32_t len)
{
    uint32_t i;
    uint32_t anss = dev->anss;
    dev->anss = 0;
    
    /* Select slave */
    sspi_select(dev, slave_nb);

    /* Send bytes */
    for(i = 0; i < len; i++)
    {
        rx_data[i] = (uint8_t)sspi_transfer(dev, 8, tx_data[i]);
    }

    /* Deselect slave */
    sspi_unselect(dev, slave_nb);

    dev->anss = anss;
}
#endif