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

#ifndef __SPLL_BACKUP_H
#define __SPLL_BACKUP_H

#include "spll_common.h"

#define BACKUP_PORTS         18

struct spll_switchover_state {
	int occured;
	int trigger_src;
	int ms_avg_long;
	int ms_avg_short;
	int bs_avg_long;
	int bs_avg_short;
	int xs_avg_long[BACKUP_PORTS];
	int xs_avg_short[BACKUP_PORTS];
	int ms_max;
	int ms_min;
	int ms_mtied;
	int ms_down_qualifier_cnt;
	int old_active_chan;
	int new_active_chan;
	uint32_t backup_mask;
};


/* State of the backup PLL */
struct spll_backup_state {
	int state;

	spll_lock_det_t ld;
	spll_avg_t avg_err_short;
	spll_avg_t avg_err_long;
	
	int adder_ref, adder_out, tag_ref, tag_out, tag_ref_d, tag_out_d;

	// tag sequencing stuff
	uint32_t seq_ref, seq_out;
	int match_state;
	int match_seq;

	int phase_shift_target;
	int phase_shift_current;
	int phase_good_val; //ps (divided)
	int id_ref, id_out;	/* IDs of the reference and the output channel */
	int sample_n;
// 	int delock_count;
// 	int dac_index;
	int enabled;
	int holdover;
	int err_d;
	int stabilize_cntdown;
// 	int err_history[ERR_HIST_LEN];
// 	int pointer;
	int priority;
	struct spll_multibackup_state *xpll;
};

void bpll_init(struct spll_backup_state *s);

void bpll_stop(struct spll_backup_state *s);

void bpll_start(struct spll_backup_state *s, int id_ref, int id_out, int priority);

int bpll_update(struct spll_backup_state *s, int tag, int source);

int bpll_set_phase_shift(struct spll_backup_state *s,
				int desired_shift_ps);

int bpll_shifter_busy(struct spll_backup_state *s);
void bpll_show_stats(struct spll_backup_state *s);
int bpll_avg_check(struct spll_backup_state *s, int *bs_avg_l, int *bs_avg_s);
#endif // __SPLL_MAIN_H
