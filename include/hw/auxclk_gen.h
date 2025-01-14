#ifndef __CHEBY__AUXCLK_GEN__H__
#define __CHEBY__AUXCLK_GEN__H__
#define AUXCLK_GEN_SIZE 8 /* 0x8 */

/* Period Register */
#define AUXCLK_GEN_PR 0x0UL
#define AUXCLK_GEN_PR_HP_WIDTH_MASK 0xffffUL
#define AUXCLK_GEN_PR_HP_WIDTH_SHIFT 0

/* Duty Cycle Register */
#define AUXCLK_GEN_DCR 0x4UL
#define AUXCLK_GEN_DCR_LOW_WIDTH_MASK 0xffffUL
#define AUXCLK_GEN_DCR_LOW_WIDTH_SHIFT 0

#ifndef __ASSEMBLER__
struct auxclk_gen {
  /* [0x0]: REG (rw) Period Register */
  uint32_t PR;

  /* [0x4]: REG (rw) Duty Cycle Register */
  uint32_t DCR;
};
#endif /* !__ASSEMBLER__*/

#endif /* __CHEBY__AUXCLK_GEN__H__ */
