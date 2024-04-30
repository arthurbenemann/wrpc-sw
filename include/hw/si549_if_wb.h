#include <stdint.h>

#ifndef __CHEBY__SI549_IF_WB__H__
#define __CHEBY__SI549_IF_WB__H__
#define SI549_IF_WB_SIZE 20 /* 0x14 */

/* Control Register */
#define SI549_IF_WB_CR 0x0UL
#define SI549_IF_WB_CR_I2C_ADDR_MASK 0xffUL
#define SI549_IF_WB_CR_I2C_ADDR_SHIFT 0
#define SI549_IF_WB_CR_ENABLE 0x100UL
#define SI549_IF_WB_CR_GAIN_MASK 0x1fe00UL
#define SI549_IF_WB_CR_GAIN_SHIFT 9
#define SI549_IF_WB_CR_CLK_DIV_MASK 0x1fe0000UL
#define SI549_IF_WB_CR_CLK_DIV_SHIFT 17
#define SI549_IF_WB_CR_BUSY 0x2000000UL

/* DAC value gain (signed Q3.13) */
#define SI549_IF_WB_GAIN 0x4UL
#define SI549_IF_WB_GAIN_GAIN_VALUE_MASK 0xffffUL
#define SI549_IF_WB_GAIN_GAIN_VALUE_SHIFT 0

/* GPIO Set/Readback Register */
#define SI549_IF_WB_GPSR 0x8UL
#define SI549_IF_WB_GPSR_SCL 0x1UL
#define SI549_IF_WB_GPSR_SDA 0x2UL

/* GPIO Clear Register */
#define SI549_IF_WB_GPCR 0xcUL
#define SI549_IF_WB_GPCR_SCL 0x1UL
#define SI549_IF_WB_GPCR_SDA 0x2UL

/* Debug register, manually set DAC value */
#define SI549_IF_WB_DEBUG 0x10UL
#define SI549_IF_WB_DEBUG_DAC_VAL_MASK 0xffffUL
#define SI549_IF_WB_DEBUG_DAC_VAL_SHIFT 0

#ifndef __ASSEMBLER__
struct si549_if_wb {
  /* [0x0]: REG (rw) Control Register */
  uint32_t CR;

  /* [0x4]: REG (rw) DAC value gain (signed Q3.13) */
  uint32_t GAIN;

  /* [0x8]: REG (rw) GPIO Set/Readback Register */
  uint32_t GPSR;

  /* [0xc]: REG (wo) GPIO Clear Register */
  uint32_t GPCR;

  /* [0x10]: REG (rw) Debug register, manually set DAC value */
  uint32_t DEBUG;
};
#endif /* !__ASSEMBLER__*/

#endif /* __CHEBY__SI549_IF_WB__H__ */
