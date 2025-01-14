#ifndef __CHEBY__NMEA_MASTER__H__
#define __CHEBY__NMEA_MASTER__H__
#define NMEA_MASTER_SIZE 16 /* 0x10 */

/* Control Register */
#define NMEA_MASTER_CR 0x0UL
#define NMEA_MASTER_CR_BAUD_DIV_MASK 0x1ffffUL
#define NMEA_MASTER_CR_BAUD_DIV_SHIFT 0
#define NMEA_MASTER_CR_BAUD_DIV_PRESET 0x3c6UL
#define NMEA_MASTER_CR_INVERT 0x20000UL

/* Status Register */
#define NMEA_MASTER_SR 0x4UL
#define NMEA_MASTER_SR_TIP 0x1UL
#define NMEA_MASTER_SR_VALID 0x2UL
#define NMEA_MASTER_SR_CLK_FREQ_MASK 0xfffffffcUL
#define NMEA_MASTER_SR_CLK_FREQ_SHIFT 2

/* Time of day hr:min:sec being sent on nmea port, fields in bcd format */
#define NMEA_MASTER_TOD 0x8UL
#define NMEA_MASTER_TOD_SECOND_MASK 0xffUL
#define NMEA_MASTER_TOD_SECOND_SHIFT 0
#define NMEA_MASTER_TOD_MINUTE_MASK 0xff00UL
#define NMEA_MASTER_TOD_MINUTE_SHIFT 8
#define NMEA_MASTER_TOD_HOUR_MASK 0xff0000UL
#define NMEA_MASTER_TOD_HOUR_SHIFT 16

/* Date, Years / Days being sent on nmea port, fields in bcd format */
#define NMEA_MASTER_DATE 0xcUL
#define NMEA_MASTER_DATE_DAY_MASK 0xffUL
#define NMEA_MASTER_DATE_DAY_SHIFT 0
#define NMEA_MASTER_DATE_MONTH_MASK 0xff00UL
#define NMEA_MASTER_DATE_MONTH_SHIFT 8
#define NMEA_MASTER_DATE_YEAR_MASK 0xffff0000UL
#define NMEA_MASTER_DATE_YEAR_SHIFT 16

#ifndef __ASSEMBLER__
struct nmea_master {
  /* [0x0]: REG (rw) Control Register */
  uint32_t CR;

  /* [0x4]: REG (ro) Status Register */
  uint32_t SR;

  /* [0x8]: REG (ro) Time of day hr:min:sec being sent on nmea port, fields in bcd format */
  uint32_t TOD;

  /* [0xc]: REG (ro) Date, Years / Days being sent on nmea port, fields in bcd format */
  uint32_t DATE;
};
#endif /* !__ASSEMBLER__*/

#endif /* __CHEBY__NMEA_MASTER__H__ */
