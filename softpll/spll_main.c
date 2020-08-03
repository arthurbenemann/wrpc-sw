/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_main.c - Implementation of the main DDMTD PLL. */

#include <wrc.h>
#include "softpll_ng.h"

#define MPLL_TAG_WRAPAROUND 100000000

#undef WITH_SEQUENCING

#ifdef CONFIG_DAC_LOG
extern void spll_log_dac(int y);
#else
static inline void spll_log_dac(int y) {}
#endif

void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out)
{
	/* Frequency branch PI controller */
	s->pi.y_min = 5;
	s->pi.y_max = 65530;
	s->pi.anti_windup = 1;
	s->pi.bias = 30000;
	s->pi.shift = PI_FRACBITS;
#if defined(CONFIG_WR_SWITCH)
	if (spll_ljd_present) {
		s->pi.kp = 2000;
		s->pi.ki = 15;
	} else {
		s->pi.kp = 1100;		// / 2;
		s->pi.ki = 30;			// / 2;
	}
#elif defined(CONFIG_WR_NODE) && !defined(CONFIG_TARGET_SPEC7)
	s->pi.kp = -1100;		// / 2;
	s->pi.ki = -30;			// / 2;
#elif defined(CONFIG_WR_NODE) && defined(CONFIG_TARGET_SPEC7)
//	s->pi.kp = -800;		// / 2;
//	s->pi.ki = -10;			// / 2;
	s->pi.kp = -5500;		// / 2;
	s->pi.ki = -30;			// / 2;
#else
#error "Please set CONFIG for wr switch or wr node"
#endif
	s->enabled = 0;

	/* Freqency branch lock detection */
	s->ld.threshold = 1200;
	s->ld.lock_samples = 1000;
	s->ld.delock_samples = 100;
	s->id_ref = id_ref;
	s->id_out = id_out;
	s->dac_index = id_out - spll_n_chan_ref;
	board_dbg("Main PLL PI Values:   Kp %i\t Ki%i\n",s->pi.kp,s->pi.ki);

	if( s->gain_sched )
	{
		s->gain_sched->current_stage = 0;
		s->gain_sched->locked_d = 0;
	}

	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);
}

static inline void mpll_handle_gain_schedule( struct spll_main_state *s )
{
	int do_update = 0;

	if (!s->gain_sched)
	{
		s->locked = s->ld.locked;
		return;
	}

	if( s->gain_sched->locked_d && !s->ld.locked ) // Pll out-of-lock? restart
	{
		s->gain_sched->current_stage = 0;
		s->locked = 0;
		do_update = 1;
	}
	else if ( !s->gain_sched->locked_d && s->ld.locked ) // PLL lock acquired? advance stage
	{
		spll_debug(DBG_EVENT | DBG_MAIN, DBG_EVT_GAIN_SWITCH, 0);
		if ( s->gain_sched->current_stage == s->gain_sched->n_stages - 1 )
		{
			s->locked = 1;
			s->gain_sched->locked_d = 1;
			return;
		}
		else
		{
			s->gain_sched->current_stage++;
		}

		do_update = 1;
	}

	if( do_update )
	{
		spll_gain_schedule_item_t* stage = &s->gain_sched->stages[ s->gain_sched->current_stage ];
		s->pi.kp = stage->kp;
		s->pi.ki = stage->ki;
		s->pi.shift = stage->shift;
		s->ld.lock_samples = stage->lock_samples;
		s->ld.lock_cnt = 0;
		s->ld.lock_changed = 0;
		s->ld.locked = 0;
		s->gain_sched->locked_d = 0;
	}

	s->gain_sched->locked_d = s->ld.locked;
}

void mpll_start(struct spll_main_state *s)
{
	pll_verbose("MPLL_Start [dac %d]\n", s->dac_index);

	s->adder_ref = s->adder_out = 0;
	s->tag_ref = -1;
	s->tag_out = -1;
	s->tag_ref_d = -1;
	s->tag_out_d = -1;

	s->phase_shift_target = 0;
	s->phase_shift_current = 0;
	s->sample_n = 0;
	s->enabled = 1;
	s->locked = 0;


	if( s->gain_sched )
	{
		s->gain_sched->current_stage = 0;
		s->gain_sched->locked_d = 0;
		s->pi.kp = s->gain_sched->stages[0].kp;
		s->pi.ki = s->gain_sched->stages[0].ki;
		s->pi.shift = s->gain_sched->stages[0].shift;
		s->ld.lock_samples = s->gain_sched->stages[0].lock_samples;
		s->ld.lock_cnt = 0;
	}


	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);

	spll_enable_tagger(s->id_ref, 1);
	spll_enable_tagger(s->id_out, 1);
	spll_debug(DBG_EVENT | DBG_MAIN, DBG_EVT_START, 1);
}

void mpll_stop(struct spll_main_state *s)
{
	spll_enable_tagger(s->id_out, 0);
	s->enabled = 0;
}

int mpll_update(struct spll_main_state *s, int tag, int source)
{
	if(!s->enabled)
	    return SPLL_LOCKED;

	int err, y;

	if (source == s->id_ref)
		s->tag_ref = tag;

	if (source == s->id_out)
		s->tag_out = tag;

	if (s->tag_ref >= 0) {
		if(s->tag_ref_d >= 0 && s->tag_ref_d > s->tag_ref)
			s->adder_ref += (1 << TAG_BITS);

		s->tag_ref_d = s->tag_ref;
	}


	if (s->tag_out >= 0) {
		if(s->tag_out_d >= 0 && s->tag_out_d > s->tag_out)
			s->adder_out += (1 << TAG_BITS);

		s->tag_out_d = s->tag_out;
	}

	if (s->tag_ref >= 0 && s->tag_out >= 0) {
		err = s->adder_ref + s->tag_ref - s->adder_out - s->tag_out;

#ifndef WITH_SEQUENCING

		/* Hack: the PLL is locked, so the tags are close to
		   each other. But when we start phase shifting, after
		   reaching full clock period, one of the reference
		   tags will flip before the other, causing a suddent
		   2**HPLL_N jump in the error.  So, once the PLL is
		   locked, we just mask out everything above
		   2**HPLL_N.

		   Proper solution: tag sequence numbers */
		if (s->locked) {
			err &= (1 << HPLL_N) - 1;
			if (err & (1 << (HPLL_N - 1)))
				err |= ~((1 << HPLL_N) - 1);
		}

#endif

		y = pi_update((spll_pi_t *)&s->pi, err);
		SPLL->DAC_MAIN = SPLL_DAC_MAIN_VALUE_W(y)
			| SPLL_DAC_MAIN_DAC_SEL_W(s->dac_index);
		if (s->dac_index == 0)
			spll_log_dac(y);

		spll_debug(DBG_MAIN | DBG_REF, s->tag_ref + s->adder_ref, 0);
		spll_debug(DBG_MAIN | DBG_TAG, s->tag_out + s->adder_out, 0);
		spll_debug(DBG_MAIN | DBG_ERR, err, 0);
		spll_debug(DBG_MAIN | DBG_SAMPLE_ID, s->sample_n++, 0);
		spll_debug(DBG_MAIN | DBG_Y, y, 1);

		s->tag_out = -1;
		s->tag_ref = -1;

		if (s->adder_ref > 2 * MPLL_TAG_WRAPAROUND
		    && s->adder_out > 2 * MPLL_TAG_WRAPAROUND) {
			s->adder_ref -= MPLL_TAG_WRAPAROUND;
			s->adder_out -= MPLL_TAG_WRAPAROUND;
		}

		if (s->locked) {
			if (s->phase_shift_current < s->phase_shift_target) {
				s->phase_shift_current++;
#if defined(CONFIG_WR_SWITCH)
				s->adder_ref++;
#else
				s->adder_ref--;
#endif
			} else if (s->phase_shift_current >
				   s->phase_shift_target) {
				s->phase_shift_current--;
#if defined(CONFIG_WR_SWITCH)
				s->adder_ref--;
#else
				s->adder_ref++;
#endif
			}
		}

		ld_update((spll_lock_det_t *)&s->ld, err);
		if( s->ld.lock_changed) 
			spll_debug(DBG_EVENT | DBG_MAIN, DBG_EVT_LOCKED, 1);

		mpll_handle_gain_schedule(s);

		if(s->locked)
			return SPLL_LOCKED;
	}

	return SPLL_LOCKING;
}

#ifdef CONFIG_PPSI /* use __div64_32 from ppsi library to save libgcc memory */
static int32_t from_picos(int32_t ps)
{
	extern uint32_t __div64_32(uint64_t *n, uint32_t base);
	uint64_t ups = ps;

	if (ps >= 0) {
		ups *= 1 << HPLL_N;
		__div64_32(&ups, CLOCK_PERIOD_PICOSECONDS);
		return ups;
	}
	ups = -ps * (1 << HPLL_N);
	__div64_32(&ups, CLOCK_PERIOD_PICOSECONDS);
	return -ups;
}
#else /* previous implementation: ptp-noposix has no __div64_32 available */
static int32_t from_picos(int32_t ps)
{
	return (int32_t) ((int64_t) ps * (int64_t) (1 << HPLL_N) /
			  (int64_t) CLOCK_PERIOD_PICOSECONDS);
}
#endif

int mpll_set_phase_shift(struct spll_main_state *s,
				int desired_shift_ps)
{
	int div = (DIVIDE_DMTD_CLOCKS_BY_2 ? 2 : 1);
	s->phase_shift_target = from_picos(desired_shift_ps) / div;
	return 0;
}

int mpll_shifter_busy(struct spll_main_state *s)
{
	return s->phase_shift_target != s->phase_shift_current;
}

