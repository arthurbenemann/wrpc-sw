#ifndef __CHEBY__TIMECODE__H__
#define __CHEBY__TIMECODE__H__

#include "auxclk_gen.h"
#include "nmea_master.h"
#define TIMECODE_SIZE 80 /* 0x50 */

/* Control Register */
#define TIMECODE_CR 0x0UL
#define TIMECODE_CR_VALID 0x1UL
#define TIMECODE_CR_SERDES_IP_SEL_MASK 0x6UL
#define TIMECODE_CR_SERDES_IP_SEL_SHIFT 1

/* Status Register */
#define TIMECODE_SR 0x4UL
#define TIMECODE_SR_VALID 0x1UL
#define TIMECODE_SR_AUXCLK_ENABLED 0x2UL
#define TIMECODE_SR_IRIG_ENABLED 0x4UL
#define TIMECODE_SR_NMEA_ENABLED 0x8UL

/* block for rw, next second registers */
#define TIMECODE_NEXT 0x10UL
#define TIMECODE_NEXT_SIZE 16 /* 0x10 */

/* Next UTC month/day/hour/minute/second */
#define TIMECODE_NEXT_UTC_MDHMS 0x10UL
#define TIMECODE_NEXT_UTC_MDHMS_SECOND_MASK 0x3fUL
#define TIMECODE_NEXT_UTC_MDHMS_SECOND_SHIFT 0
#define TIMECODE_NEXT_UTC_MDHMS_MINUTE_MASK 0xfc0UL
#define TIMECODE_NEXT_UTC_MDHMS_MINUTE_SHIFT 6
#define TIMECODE_NEXT_UTC_MDHMS_UTC_HR_MASK 0x3f000UL
#define TIMECODE_NEXT_UTC_MDHMS_UTC_HR_SHIFT 12
#define TIMECODE_NEXT_UTC_MDHMS_UTC_DAY_MASK 0x7c0000UL
#define TIMECODE_NEXT_UTC_MDHMS_UTC_DAY_SHIFT 18
#define TIMECODE_NEXT_UTC_MDHMS_UTC_MON_MASK 0x7800000UL
#define TIMECODE_NEXT_UTC_MDHMS_UTC_MON_SHIFT 23

/* Next UTC year */
#define TIMECODE_NEXT_UTC_Y 0x14UL
#define TIMECODE_NEXT_UTC_Y_UTC_YEAR_MASK 0xfffUL
#define TIMECODE_NEXT_UTC_Y_UTC_YEAR_SHIFT 0
#define TIMECODE_NEXT_UTC_Y_UTC_DIY_MASK 0x1ff000UL
#define TIMECODE_NEXT_UTC_Y_UTC_DIY_SHIFT 12

/* Next UTC sbs */
#define TIMECODE_NEXT_UTC_SBS 0x18UL
#define TIMECODE_NEXT_UTC_SBS_UTC_SBS_MASK 0x1ffffUL
#define TIMECODE_NEXT_UTC_SBS_UTC_SBS_SHIFT 0

/* Next leap second */
#define TIMECODE_NEXT_LEAP_SEC 0x1cUL
#define TIMECODE_NEXT_LEAP_SEC_LS_VALUE_MASK 0xffUL
#define TIMECODE_NEXT_LEAP_SEC_LS_VALUE_SHIFT 0
#define TIMECODE_NEXT_LEAP_SEC_LS_FLAG59 0x100UL
#define TIMECODE_NEXT_LEAP_SEC_LS_FLAG61 0x200UL
#define TIMECODE_NEXT_LEAP_SEC_LS_VALID 0x400UL

/* block for ro, current second registers */
#define TIMECODE_CURR 0x20UL
#define TIMECODE_CURR_SIZE 16 /* 0x10 */

/* Current UTC month/day/hour/minute/second */
#define TIMECODE_CURR_UTC_MDHMS 0x20UL
#define TIMECODE_CURR_UTC_MDHMS_SECOND_MASK 0x3fUL
#define TIMECODE_CURR_UTC_MDHMS_SECOND_SHIFT 0
#define TIMECODE_CURR_UTC_MDHMS_MINUTE_MASK 0xfc0UL
#define TIMECODE_CURR_UTC_MDHMS_MINUTE_SHIFT 6
#define TIMECODE_CURR_UTC_MDHMS_UTC_HR_MASK 0x3f000UL
#define TIMECODE_CURR_UTC_MDHMS_UTC_HR_SHIFT 12
#define TIMECODE_CURR_UTC_MDHMS_UTC_DAY_MASK 0x7c0000UL
#define TIMECODE_CURR_UTC_MDHMS_UTC_DAY_SHIFT 18
#define TIMECODE_CURR_UTC_MDHMS_UTC_MON_MASK 0x7800000UL
#define TIMECODE_CURR_UTC_MDHMS_UTC_MON_SHIFT 23

/* Current UTC year */
#define TIMECODE_CURR_UTC_Y 0x24UL
#define TIMECODE_CURR_UTC_Y_UTC_YEAR_MASK 0xfffUL
#define TIMECODE_CURR_UTC_Y_UTC_YEAR_SHIFT 0
#define TIMECODE_CURR_UTC_Y_UTC_DIY_MASK 0x1ff000UL
#define TIMECODE_CURR_UTC_Y_UTC_DIY_SHIFT 12

/* Current UTC sbs/diy */
#define TIMECODE_CURR_UTC_SBS 0x28UL
#define TIMECODE_CURR_UTC_SBS_UTC_SBS_MASK 0x1ffffUL
#define TIMECODE_CURR_UTC_SBS_UTC_SBS_SHIFT 0

/* Current leap second */
#define TIMECODE_CURR_LEAP_SEC 0x2cUL
#define TIMECODE_CURR_LEAP_SEC_LS_VALUE_MASK 0xffUL
#define TIMECODE_CURR_LEAP_SEC_LS_VALUE_SHIFT 0
#define TIMECODE_CURR_LEAP_SEC_LS_FLAG59 0x100UL
#define TIMECODE_CURR_LEAP_SEC_LS_FLAG61 0x200UL
#define TIMECODE_CURR_LEAP_SEC_LS_VALID 0x400UL

/* auxclk generator interface submap */
#define TIMECODE_AUXCLK 0x30UL
#define ADDR_MASK_TIMECODE_AUXCLK 0x78UL
#define TIMECODE_AUXCLK_SIZE 8 /* 0x8 */

/* nmea master interface submap */
#define TIMECODE_NMEA 0x40UL
#define ADDR_MASK_TIMECODE_NMEA 0x70UL
#define TIMECODE_NMEA_SIZE 16 /* 0x10 */

#ifndef __ASSEMBLER__
struct timecode {
  /* [0x0]: REG (rw) Control Register */
  uint32_t CR;

  /* [0x4]: REG (ro) Status Register */
  uint32_t SR;

  /* padding to: 16 Bytes */
  uint32_t __padding_0[2];

  /* [0x10]: BLOCK block for rw, next second registers */
  struct NEXT {
    /* [0x0]: REG (rw) Next UTC month/day/hour/minute/second */
    uint32_t UTC_MDHMS;

    /* [0x4]: REG (rw) Next UTC year */
    uint32_t UTC_Y;

    /* [0x8]: REG (rw) Next UTC sbs */
    uint32_t UTC_SBS;

    /* [0xc]: REG (rw) Next leap second */
    uint32_t LEAP_SEC;
  } NEXT;

  /* [0x20]: BLOCK block for ro, current second registers */
  struct CURR {
    /* [0x0]: REG (ro) Current UTC month/day/hour/minute/second */
    uint32_t UTC_MDHMS;

    /* [0x4]: REG (ro) Current UTC year */
    uint32_t UTC_Y;

    /* [0x8]: REG (ro) Current UTC sbs/diy */
    uint32_t UTC_SBS;

    /* [0xc]: REG (ro) Current leap second */
    uint32_t LEAP_SEC;
  } CURR;

  /* [0x30]: SUBMAP auxclk generator interface submap */
  struct auxclk_gen auxclk;

  /* padding to: 64 Bytes */
  uint32_t __padding_1[2];

  /* [0x40]: SUBMAP nmea master interface submap */
  struct nmea_master nmea;
};
#endif /* !__ASSEMBLER__*/

#endif /* __CHEBY__TIMECODE__H__ */
