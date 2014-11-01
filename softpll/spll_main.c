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
#include "softpll_ng.h"
#include <pp-printf.h>
#include "trace.h"
#include "irq.h"
#define MPLL_TAG_WRAPAROUND 100000000

#define MATCH_NEXT_TAG 0
#define MATCH_WAIT_REF 1
#define MATCH_WAIT_OUT 2

#undef WITH_SEQUENCING

void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out)
{
	/* Frequency branch PI controller */
	s->pi.y_min = 5;
	s->pi.y_max = 65530;
	s->pi.anti_windup = 1;
	s->pi.bias = 30000;
#if defined(CONFIG_WR_SWITCH)
	s->pi.kp = 1100;		// / 2;
	s->pi.ki = 30;			// / 2;
#elif defined(CONFIG_WR_NODE)
	s->pi.kp = 1100;		// / 2;
	s->pi.ki = 30;			// / 2;
#else
#error "Please set CONFIG for wr switch or wr node"
#endif
	s->delock_count = 0;
	s->enabled = 0;

	/* Freqency branch lock detection */
	s->ld.threshold = 1200;
	s->ld.lock_samples = 1000;
	s->ld.delock_samples = 100;
	s->id_ref = id_ref;
	s->id_out = id_out;
	s->holdover=0;
	s->holdover_cnt=0;
	s->dac_index = id_out - spll_n_chan_ref;

	TRACE_DEV("ref %d out %d idx %x", s->id_ref, s->id_out, s->dac_index);

	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);
	
}

void mpll_start(struct spll_main_state *s)
{
// 	TRACE_DEV("MPLL_Start [dac %d]\n", s->dac_index);

	s->adder_ref = s->adder_out = 0;
	s->tag_ref = -1;
	s->tag_out = -1;
	s->tag_ref_d = -1;
	s->tag_out_d = -1;
	s->seq_ref = 0;
	s->seq_out = 0;
	s->err_d = 0;
	s->match_state = MATCH_NEXT_TAG;

	s->phase_shift_target = 0;
	s->phase_shift_current = 0;
	s->phase_good_val=-1;
	s->after_switchover = 0;
	s->sample_n = 0;
	s->enabled = 1;
	s->fifo=0;
	s->holdover=0;
	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);
	
	//average
	avg_init((spll_avg_t *)&s->avg_err_short,4);
	avg_init((spll_avg_t *)&s->avg_err_long ,10);
	avg_init((spll_avg_t *)&s->avg_y_long   ,7);
	s->max = 0;
	s->min = 0;
	s->mtie_d = 0;
	s->down_qulifier = 0;
	s->down_qulifier_cnt = 0;
// 	avg_dump((spll_avg_t *)&s->avg_err_short, "init ERR short");
// 	avg_dump((spll_avg_t *)&s->avg_err_long,  "init ERR long ");
// 	avg_dump((spll_avg_t *)&s->avg_y_long,    "init Y   long ");

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
	int en;
	int32_t phase=0;	

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
		if (s->ld.locked) {
			err &= (1 << HPLL_N) - 1;
			if (err & (1 << (HPLL_N - 1)))
				err |= ~((1 << HPLL_N) - 1);
		}

#endif
		if(s->holdover==0)
		    s->err_d = err;
		avg_update((spll_avg_t *)&s->avg_err_short, err);
		avg_update((spll_avg_t *)&s->avg_err_long,  err);
// 		s->err_history[s->pointer++] = err;
//  		if(s->pointer == ERR_HIST_LEN) s->pointer = 0;
// 		if ((s->ld.locked && abs(err) < 50) || ! s->ld.locked || s->after_switchover)
		{
			if(s->holdover==0)
				y = pi_update((spll_pi_t *)&s->pi, err);
			else
				y = avg_get((spll_avg_t *)&s->avg_y_long, AVG_HIST_OLDEST);
			
			SPLL->DAC_MAIN = SPLL_DAC_MAIN_VALUE_W(y)
			      | SPLL_DAC_MAIN_DAC_SEL_W(s->dac_index);
			if(abs(err)<50 && s->ld.locked ) 
				s->after_switchover = 0;
			if(s->holdover==0)
				avg_update((spll_avg_t *)&s->avg_y_long, y);
		}
		
// 		spll_debug(DBG_MAIN | DBG_REF, s->tag_ref + s->adder_ref, 0);
// 		spll_debug(DBG_MAIN | DBG_TAG, s->tag_out + s->adder_out, 0);
		spll_debug(DBG_MAIN | DBG_REF,   avg_get((spll_avg_t *)&s->avg_y_long, AVG_HIST_OLDEST), 0);
		spll_debug(DBG_MAIN | DBG_TAG,   avg_get((spll_avg_t *)&s->avg_y_long, AVG_HIST_RECENT), 0);
		spll_debug(DBG_MAIN | DBG_AVG_L, avg_get((spll_avg_t *)&s->avg_err_long, AVG_HIST_RECENT), 0);
		spll_debug(DBG_MAIN | DBG_AVG_S, avg_get((spll_avg_t *)&s->avg_err_short, AVG_HIST_RECENT), 0);
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
		{
			if(s->ld.lock_cnt == s->ld.lock_samples)
			{
			    spll_read_ptracker(s->id_ref, &phase, &en);
			    if(en && phase) s->phase_good_val = phase;
			}
			return SPLL_LOCKED;
		}
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

int mpll_down(struct spll_main_state *s, uint32_t hw_st)
{
	int hw_status   = 0x1 & (hw_st >> s->id_ref);
	int hw_status_d = s->hw_status_d;
	s->hw_status_d  = hw_status;
	if(hw_status_d == 1 && hw_status == 0) // link went down
	    return 1;
	return 0;
}

int mpll_switchover(struct spll_main_state *mpll, struct spll_backup_state *bpll, int phase_val)
{
	int i;
	/*switch over between bpll and mpll by copying the appropriate runtime and config
	  data 
	  TODO: copying of the config data probably not needed, but we need to ensure
	  this is the same, it seems
	  TODO: this function might need to get more universal, if possible, to enable
	        switching over between backup port that is now being active and a newly
	        up port which should be the active one (prio=0). In other words, we want to 
	        switchover between working ports. this should be able having the new port 
	        first ackt as a backup, intill all runtime parameters are learnt, then 
	        using this function to switchover.
	*/
// 	disable_irq();
	mpll->adder_ref           = from_picos((bpll->phase_good_val % 16000));//bpll->adder_ref;
	mpll->adder_out           = 0; //bpll->adder_out;
	mpll->tag_ref             = bpll->tag_ref;
	mpll->tag_out             = bpll->tag_out;
	mpll->tag_ref_d           = bpll->tag_ref_d;
	mpll->tag_out_d           = bpll->tag_out_d;
	mpll->seq_ref             = bpll->seq_ref;
	/*
	 * Here is the intent:
	 * - we set the measured phase value as the setpoint, this is to avoid jumps (we start
	 *   with what is there.
	 * - we let the PTP to calculate the setpoint after the switch over,
	 * - the "correct" setpoint will be insterted as target, therefore it should 
	 *   be smoothly applied
	 */

	mpll->phase_shift_target  = from_picos((bpll->phase_good_val % 16000));//from_picos((phase_val % 16000));
	mpll->phase_shift_current = from_picos((bpll->phase_good_val % 16000));//from_picos((phase_val % 16000));
	mpll->phase_good_val      = bpll->phase_good_val;
	mpll->after_switchover    = 1;
	/******************** end of interest *********************/
	mpll->id_out              = bpll->id_out;
	mpll->id_ref              = bpll->id_ref;
// 	mpll->delock_count        = bpll->delock_count;
// 	mpll->dac_index           = bpll->dac_index;
	mpll->enabled             = 1;//bpll->enabled;
	mpll->err_d               = bpll->err_d;
	mpll->ld.lock_cnt         = mpll->ld.lock_samples;
	mpll->hw_status_d         = 1; //up
	/*stop bpll*/
	bpll->adder_ref           = 0;
	bpll->adder_out           = 0;
	bpll->tag_ref             = -1;
	bpll->tag_out             = -1;
	bpll->tag_ref_d           = -1;
	bpll->tag_out_d           = -1;
	bpll->seq_ref             = 0;
	bpll->phase_shift_target  = 0;
	bpll->phase_shift_current = 0;
	bpll->id_out              = 0;
	bpll->id_ref              = 0;
// 	bpll->delock_count        = 0;
// 	bpll->dac_index           = 0;
	bpll->enabled             = 0;
	bpll->err_d               = 0;
// 	enable_irq();
	
	mpll->holdover = 0;
	bpll->holdover = 0;
	mpll->holdover_cnt=0;
	bpll->ld.locked = 0;
	
// 	rts_update();
//         enable_irq();

	return 0;
}
