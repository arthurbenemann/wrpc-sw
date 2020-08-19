/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_main.h - the main DDMTD PLL. Locks output clock to any reference
   with programmable phase shift. */

#ifndef __SPLL_MAIN_H
#define __SPLL_MAIN_H

#include "spll_common.h"

#define HO_BUF_LEN 128

typedef struct{
	int dataspace[HO_BUF_LEN];
	int * buffer;
	int head;
	int tail;
	int max_length;
} holdover_buffer_t;

/* State of the Main PLL */
struct spll_main_state {
	int state;

	spll_pi_t pi;
	spll_lock_det_t ld;

	holdover_buffer_t ho_buf_y; // corrections sent to DAC
	holdover_buffer_t ho_buf_x; // error measured by the DMTD
	int ho_buf_div;
	int ho_func_sel;
	int ho_lrn_active; 

	int adder_ref, adder_out, tag_ref, tag_out, tag_ref_d, tag_out_d;

	// tag sequencing stuff
	uint32_t seq_ref, seq_out;
	int match_state;
	int match_seq;

	int phase_shift_target;
	int phase_shift_current;
	int id_ref, id_out;	/* IDs of the reference and the output channel */
	int sample_n;
	int delock_count;
	int dac_index;
	int enabled;
};

void mpll_init(struct spll_main_state *s, int id_ref,
		      int id_out, int mode);

void mpll_stop(struct spll_main_state *s);

void mpll_start(struct spll_main_state *s);

int mpll_update(struct spll_main_state *s, int tag, int source);

int mpll_set_phase_shift(struct spll_main_state *s,
				int desired_shift_ps);

int mpll_shifter_busy(struct spll_main_state *s);

int mpll_get_pi(struct spll_main_state *s, int param);

int mpll_set_pi(struct spll_main_state *s, int param, int value);

int ho_buf_push(holdover_buffer_t *buf, int data);

int ho_buf_pop(holdover_buffer_t *buf, int *data);

int ho_update(struct spll_main_state *s);

int ho_update_0(struct spll_main_state *s);

int ho_update_1(struct spll_main_state *s);

int ho_update_2(struct spll_main_state *s);

int ho_update_3(struct spll_main_state *s);

#endif // __SPLL_MAIN_H
