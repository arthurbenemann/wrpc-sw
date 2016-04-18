/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_main.c - Implementation of the main DDMTD PLL. */

#include "spll_main.h"
#include "spll_debug.h"
#include <pp-printf.h>
#include "trace.h"

#define MPLL_TAG_WRAPAROUND 100000000

#define MATCH_NEXT_TAG 0
#define MATCH_WAIT_REF 1
#define MATCH_WAIT_OUT 2

#undef WITH_SEQUENCING

void mpll_init_tag_history (struct spll_main_state *s) {
  	s->ref_data = 0; s->out_data = 0;
	s->read_i = 0; s->ref_write_i = 0; s->out_write_i = 0;
	s->skip_initial = 1;
	
	// Just init the last one
	s->ref_tags[TAG_HISTORY-1].adder = 0;
	s->ref_tags[TAG_HISTORY-1].tag = 0;	
	s->out_tags[TAG_HISTORY-1].adder = 0;
	s->out_tags[TAG_HISTORY-1].tag = 0;
}

void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out)
{
	/* Frequency branch PI controller */
	s->pi.y_min = 5;
	s->pi.y_max = 65530;
	s->pi.anti_windup = 1;
	s->pi.bias = 30000;
#if defined(CONFIG_WR_SWITCH)
	s->pi.kp = 5000;		// / 2;
	s->pi.ki = 1;			// / 2;
#elif defined(CONFIG_WR_NODE)
	s->pi.kp = 1100;		// / 2;
	s->pi.ki = 30;			// / 2;
#else
#error "Please set CONFIG for wr switch or wr node"
#endif
	s->delock_count = 0;
	s->enabled = 0;

	/* Freqency branch lock detection */
	s->ld.threshold = 4000;
	s->ld.lock_samples = 3814*3;
	s->ld.delock_samples = 100;
	
	s->ld.avg_acc = 0;
	s->ld.call_count = 0;
	s->ld.avg_ready = 0;
	s->ld.avg_value = 0;
	
	s->id_ref = id_ref;
	s->id_out = id_out;
	s->dac_index = id_out - spll_n_chan_ref;

	TRACE_DEV("ref %d out %d idx %x \n", s->id_ref, s->id_out, s->dac_index);

	mpll_init_tag_history(s);
	
	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);
}

void mpll_start(struct spll_main_state *s)
{
	TRACE_DEV("MPLL_Start [dac %d]\n", s->dac_index);

	s->adder_ref = s->adder_out = 0;
	s->tag_ref = -1;
	s->tag_out = -1;
	s->tag_ref_d = -1;
	s->tag_out_d = -1;
	s->seq_ref = 0;
	s->seq_out = 0;
	s->match_state = MATCH_NEXT_TAG;

	s->phase_shift_target = 0;
	s->phase_shift_current = 0;
	s->sample_n = 0;
	s->enabled = 1;
	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);

	mpll_init_tag_history(s);
	
	s->ld.avg_acc = 0;
	s->ld.call_count = 0;
	s->ld.avg_ready = 0;
	s->ld.avg_value = 0;
	
	spll_enable_tagger(s->id_ref, 1);
	spll_enable_tagger(s->id_out, 1);
	spll_debug(DBG_EVENT | DBG_MAIN, DBG_EVT_START, 1);
}

void mpll_stop(struct spll_main_state *s)
{
	spll_enable_tagger(s->id_out, 0);
	s->enabled = 0;
}

//Define multi Unit Interval Tracking
#define MULTIUI

int mpll_update(struct spll_main_state *s, int tag, int source)
{
	if(!s->enabled)
	    return SPLL_LOCKED;

	int err, y;
	struct spll_tag *prev_tag;

	if (source == s->id_ref) {
	  prev_tag = &s->ref_tags[WRAP(s->ref_write_i-1,TAG_HISTORY)];
	  s->ref_tags[s->ref_write_i].tag = tag;
	  if (prev_tag->tag > tag) {
	    s->ref_tags[s->ref_write_i].adder =  prev_tag->adder + (1 << TAG_BITS);
	  } else s->ref_tags[s->ref_write_i].adder = prev_tag->adder;
		  
	  s->ref_write_i = WRAP(s->ref_write_i+1, TAG_HISTORY);
	  s->ref_data++;	
	}
	if (source == s->id_out) {
	  prev_tag = &s->out_tags[WRAP(s->out_write_i-1,TAG_HISTORY)];
	  s->out_tags[s->out_write_i].tag = tag;
	  if (prev_tag->tag > tag) {
	    s->out_tags[s->out_write_i].adder =  prev_tag->adder + (1 << TAG_BITS);
	  } else s->out_tags[s->out_write_i].adder = prev_tag->adder;
		
	  s->out_write_i = WRAP(s->out_write_i+1, TAG_HISTORY);
	  s->out_data++;
	}

	
	if (source == s->id_ref) {
		s->tag_ref = tag;
		
	}

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

#ifndef MULTIUI
	if (s->tag_ref >= 0 && s->tag_out >= 0) {
#else
	if (s->ref_data > 0 && s->out_data > 0 && s->skip_initial==0) {
#endif	  
	  
	  
	  
	  uint8_t read_p = WRAP(s->read_i, TAG_HISTORY);
#ifdef MULTIUI	
	err = s->ref_tags[read_p].adder +  s->ref_tags[read_p].tag + s->phase_shift_current -
		  s->out_tags[read_p].adder -  s->out_tags[read_p].tag;
#else		  
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
		if (s->ld.locked) {
		  
			err &= (1 << HPLL_N) - 1;
			if (err & (1 << (HPLL_N - 1)))
				err |= ~((1 << HPLL_N) - 1);
		}

#endif
#endif

#define MAIN_ERROR_CLAMP 1000000  
		 if (MAIN_ERROR_CLAMP) {
			if (err < -MAIN_ERROR_CLAMP)
				err = -MAIN_ERROR_CLAMP;
			if (err > MAIN_ERROR_CLAMP)
				err = MAIN_ERROR_CLAMP;
		}

		y = pi_update((spll_pi_t *)&s->pi, err);
		SPLL->DAC_MAIN = SPLL_DAC_MAIN_VALUE_W(y)
			| SPLL_DAC_MAIN_DAC_SEL_W(s->dac_index);

		spll_debug(DBG_MAIN | DBG_REF, s->tag_ref + s->adder_ref, 0);
		spll_debug(DBG_MAIN | DBG_TAG, s->tag_out + s->adder_out, 0);
		spll_debug(DBG_MAIN | DBG_ERR, err, 0);
		spll_debug(DBG_MAIN | DBG_SAMPLE_ID, s->sample_n++, 0);
		spll_debug(DBG_MAIN | DBG_Y, y, 1);

		s->tag_out = -1;
		s->tag_ref = -1;

		
		if (s->ref_tags[read_p].adder > 2 * MPLL_TAG_WRAPAROUND
		    && s->out_tags[read_p].adder > 2 * MPLL_TAG_WRAPAROUND) {
			int i;
			s->adder_ref -= MPLL_TAG_WRAPAROUND;
			s->adder_out -= MPLL_TAG_WRAPAROUND;
			
			for (i=0; i < TAG_HISTORY; i++) {
			 s->ref_tags[i].adder -= MPLL_TAG_WRAPAROUND;
			 s->out_tags[i].adder -= MPLL_TAG_WRAPAROUND;
			}
		}

		s->read_i = WRAP(s->read_i+1, TAG_HISTORY);
		s->ref_data--; s->out_data--;
		if (s->ld.locked) {
			if (s->phase_shift_current < s->phase_shift_target) {
				s->phase_shift_current++;
				s->adder_ref++;
			} else if (s->phase_shift_current >
				   s->phase_shift_target) {
				s->phase_shift_current--;
				s->adder_ref--;
			}
		}
		if (ld_update((spll_lock_det_t *)&s->ld, err))
			return SPLL_LOCKED;
	} else if (s->skip_initial ==1 && s->ref_data > 0 && s->out_data > 0 ) {
	    mpll_init_tag_history(s);
	    s->skip_initial = 0;
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
	TRACE_DEV("Phase shift  %d \n", s->phase_shift_target);
	return 0;
}

int mpll_shifter_busy(struct spll_main_state *s)
{
	return s->phase_shift_target != s->phase_shift_current;
}
