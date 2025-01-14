/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2024 CERN (www.cern.ch)
 * Author: Harvey Leicester <harvey.leicester@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* based on wrs_auxclk by Grzegorz Daniluk */

#include <wrc.h>
#include "board.h"
#include "dev/auxclk.h"
#include "pp-printf.h"
#include <hw/timecode_regs.h>

#define NS_FACTOR 1000000000
#define MAX_FREQ  250000000
#define MIN_FREQ  1
#define CNT_RES   2
#define AUXCLK ((volatile struct auxclk_gen*) (BASE_TIMECODE+TIMECODE_AUXCLK))

/*
 * configure auxclk output
 * freq (Hz) duty (%)
 */
int auxclk_init(uint32_t freq, uint32_t duty)
{

  uint32_t calc_freq = freq;
  uint32_t calc_duty = duty;
  uint32_t period_ns, h_width, l_width;

  if (calc_freq > MAX_FREQ)
    calc_freq = MAX_FREQ;
  if (calc_freq < MIN_FREQ)
    calc_freq = MIN_FREQ;

  if (!(calc_duty > 0 && calc_duty < 100))
    calc_duty = 50;

  period_ns = NS_FACTOR/calc_freq;
  h_width = (period_ns/CNT_RES)*(((float)calc_duty/(float)100));
  l_width = (period_ns/CNT_RES) - h_width;

  calc_duty = (h_width*100)/(period_ns/CNT_RES);

  if(NS_FACTOR/freq != period_ns || duty != calc_duty){
    pp_printf("%s: unable to configure frequency %iHz duty %i%%\n", __func__, freq, duty);
    pp_printf("%s: configuring with frequency %iHz duty %i%%\n", __func__, calc_freq, calc_duty);
  }
  AUXCLK->PR = h_width;
  AUXCLK->DCR = l_width;
  return 0;
}

int auxclk_get_settings(uint32_t *freq, uint32_t *duty)
{

  uint32_t h_width = AUXCLK->PR;
  uint32_t l_width = AUXCLK->DCR;

  uint32_t period_ns = (h_width + l_width) / CNT_RES;
  *freq = NS_FACTOR / period_ns;
  *duty = (h_width*100)/(period_ns/CNT_RES);
  return 0;
}