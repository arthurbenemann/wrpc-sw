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

#define MATCH_NEXT_TAG 0
#define MATCH_WAIT_REF 1
#define MATCH_WAIT_OUT 2

#undef WITH_SEQUENCING


void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out, int mode)
{
	/* Frequency branch PI controller */
	s->pi.y_min = 50;
	s->pi.y_max = 65485;
	s->pi.anti_windup = 1;
	s->pi.bias = 30000;

	if(mode == SPLL_MODE_GRAND_MASTER)
	{
		#if defined(CONFIG_WR_SWITCH)
			s->pi.kp = 1100;
			s->pi.ki = 30;
		#elif defined(CONFIG_WR_NODE)
			s->pi.kp = -1100;
			s->pi.ki = -30;
		#else
		#error "Please set CONFIG wr wr switch or wr node"
		#endif
	}else{
		#if defined(CONFIG_WR_SWITCH)
			s->pi.kp = 600;
			s->pi.ki = 2;
		#elif defined(CONFIG_WR_NODE)
			s->pi.kp = -600;
			s->pi.ki = -2;
		#else
		#error "Please set CONFIG wr wr switch or wr node"
		#endif
	}
	s->delock_count = 0;
	s->enabled = 0;

	/* Freqency branch lock detection */
	s->ld.threshold = 1200;
	s->ld.lock_samples = 1000;
	s->ld.delock_samples = 100;
	s->id_ref = id_ref;
	s->id_out = id_out;
	s->dac_index = id_out - spll_n_chan_ref;
	s->ld.ho_active = 0;

	s->ho_buf_y.buffer = s->ho_buf_y.dataspace;
	s->ho_buf_y.tail = 0;
	s->ho_buf_y.head=0;
	s->ho_buf_y.max_length=HO_BUF_LEN;

	s->ho_buf_x.buffer = s->ho_buf_x.dataspace;
	s->ho_buf_x.tail = 0;
	s->ho_buf_x.head=0;
	s->ho_buf_x.max_length=HO_BUF_LEN;

	s->ho_buf_div=256;
	SPLL->HO_RATE=256;

	pll_verbose("mpll_init: ref %d out %d idx %x \n", s->id_ref, s->id_out, s->dac_index);

	pi_init((spll_pi_t *)&s->pi);
	ld_init((spll_lock_det_t *)&s->ld);

}

void mpll_start(struct spll_main_state *s)
{
	pll_verbose("MPLL_Start [dac %d]\n", s->dac_index);

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
		if(s->tag_ref_d >= 0 && s->tag_ref_d > s->tag_ref){
			 s->adder_ref += (1 << TAG_BITS);
		}


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

		if (err > ((1 << TAG_BITS) - (1 << HPLL_N)))
		{
			err -= (1 << TAG_BITS);
		}

		y = pi_update((spll_pi_t *)&s->pi, err);
		// SPLL->DAC_MAIN = SPLL_DAC_MAIN_VALUE_W(y)
		// 	| SPLL_DAC_MAIN_DAC_SEL_W(s->dac_index);
		if(s->ld.ho_active){
			if( !(s->sample_n % s->ho_buf_div) )
				y = ho_update(s);
			// ho_buf_pop(&s->ho_buf, &ho_value);
			//s->pi.y=ho_value;
			SPLL->DAC_HO = SPLL_DAC_HO_VALUE_W(y);
		}else{
			if( !(s->sample_n % s->ho_buf_div) && s->ho_lrn_active )
			{
				ho_buf_push(&s->ho_buf_y, y);
				ho_buf_push(&s->ho_buf_x, err);
			}

			SPLL->DAC_HO = (y & 0xffff);
		}


		spll_debug(DBG_MAIN | DBG_REF, s->tag_ref, 0); // + s->adder_ref, 0);
		spll_debug(DBG_MAIN | DBG_TAG, s->tag_out, 0); // + s->adder_out, 0);
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
		if (ld_update((spll_lock_det_t *)&s->ld, err))
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

int mpll_get_pi(struct spll_main_state *s, int param)
{
	if( param == SPLL_KP )
		return s->pi.kp;
	else if( param == SPLL_KI )
		return s->pi.ki;
	else
		return -1;
}

int mpll_set_pi(struct spll_main_state *s, int param, int value)
{
	if(param == SPLL_KP)
	{
		s->pi.kp = value;
		return 0;
	}
	if(param == SPLL_KI)
	{
		s->pi.ki = value;
		return 0;
	}
	return -1;
}

int ho_buf_push(holdover_buffer_t *buf, int data)
{
	int next;

	next = buf->head + 1;
	if(next >= buf->max_length)
		next = 0;

	buf->buffer[buf->head] = data;
	buf->head = next;
	return 0;
}

int ho_buf_pop(holdover_buffer_t *buf, int *data)
{
	int next;

	next = buf->tail + 1;
	if(next >= buf->max_length)
		next=0;

	*data = buf->buffer[buf->tail];
	buf->tail = next;
	return 0;
}

int ho_update(struct spll_main_state *s)
{
	switch (s->ho_func_sel)
	{
	case 0:
		return ho_update_0(s);

	case 1:
		return ho_update_1(s);

	case 2:
		return ho_update_2(s);

	case 3:
		return ho_update_3(s);
	
	default:
		return ho_update_0(s);
	}

	return 30000;

}

int ho_update_0(struct spll_main_state *s)
{
	return 36500;
}

int ho_update_1(struct spll_main_state *s)
{
	int y;
	ho_buf_pop(&s->ho_buf_y, &y);
	return y;
}

int ho_update_2(struct spll_main_state *s)
{
	return 30000;
}

int ho_update_3(struct spll_main_state *s)
{
	return 30000;
}