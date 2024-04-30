#include <stdint.h>

#ifndef __CHEBY__SPI__H__
#define __CHEBY__SPI__H__
#define SPI_SIZE 28 /* 0x1c */

/* TX/RX 0 */
#define SPI_TX_RX_0 0x0UL

/* TX/RX 1 */
#define SPI_TX_RX_1 0x4UL

/* TX/RX 2 */
#define SPI_TX_RX_2 0x8UL

/* TX/RX 3 */
#define SPI_TX_RX_3 0xcUL

/* Control register */
#define SPI_CTRL 0x10UL
#define SPI_CTRL_LEN_MASK 0x7fUL
#define SPI_CTRL_LEN_SHIFT 0
#define SPI_CTRL_GO 0x100UL
#define SPI_CTRL_RX_NEGEDGE 0x200UL
#define SPI_CTRL_TX_NEGEDGE 0x400UL
#define SPI_CTRL_LSB 0x800UL
#define SPI_CTRL_IRQ 0x1000UL
#define SPI_CTRL_ASS 0x2000UL

/* Divider */
#define SPI_DIVIDER 0x14UL
#define SPI_DIVIDER_VALUE_MASK 0xffffUL
#define SPI_DIVIDER_VALUE_SHIFT 0

/* Select SPI slave */
#define SPI_SS 0x18UL
#define SPI_SS_VALUE 0x1UL

#ifndef __ASSEMBLER__
struct spi {
  /* [0x0]: REG (rw) TX/RX 0 */
  uint32_t tx_rx_0;

  /* [0x4]: REG (rw) TX/RX 1 */
  uint32_t tx_rx_1;

  /* [0x8]: REG (rw) TX/RX 2 */
  uint32_t tx_rx_2;

  /* [0xc]: REG (rw) TX/RX 3 */
  uint32_t tx_rx_3;

  /* [0x10]: REG (rw) Control register */
  uint32_t ctrl;

  /* [0x14]: REG (rw) Divider */
  uint32_t divider;

  /* [0x18]: REG (rw) Select SPI slave */
  uint32_t ss;
};
#endif /* !__ASSEMBLER__*/

#endif /* __CHEBY__SPI__H__ */
