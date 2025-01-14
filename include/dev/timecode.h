/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __TIMECODE_H
#define __TIMECODE_H

#include <stdint.h>
#include <hw/timecode_regs.h>

#define TIMECODE_SEL_CLK  (uint32_t)0
#define TIMECODE_SEL_IRIG (uint32_t)1
#define TIMECODE_SEL_NMEA (uint32_t)2

struct wr_timecode_fields {
  uint32_t sec;
  uint32_t min;
  uint32_t hour;
  uint32_t sbs;
  uint32_t day;
  uint32_t month;
  uint32_t year;
  uint32_t diy;
  uint32_t ls_val;
  uint32_t ls_flag59;
  uint32_t ls_flag61;
  uint32_t ls_valid;
  uint32_t utc_valid;
};

int timecode_update(void);
void timecode_get_tc(struct wr_timecode_fields *tc);
int timecode_sel(uint32_t ip);
int timecode_get_sel(void);
int timecode_is_enabled(uint32_t ip);

#endif