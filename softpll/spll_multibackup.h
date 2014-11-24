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

#ifndef __SPLL_MULTIBACKUP_H
#define __SPLL_MULTIBACKUP_H

#include "spll_common.h"
#define BACKUP_PORTS         18
#define BACKUP_ENTRIES_NUM  (BACKUP_PORTS+1)
#define BACKUP_EMPTY_ID      BACKUP_PORTS
#define BACKUP_PRIO_NUM      3
#define BACKUP_PRIO_PORT_NUM 6
/* State of the backup PLL */
struct spll_multibackup_state {
// 	int bids[BACKUP_PRIO_NUM][BACKUP_PRIO_PORT_NUM];
	int bids[BACKUP_ENTRIES_NUM];
	int backup_number;
	uint32_t backup_mask;
	struct spll_backup_state bpll[BACKUP_ENTRIES_NUM]; // backup main pll
};

void xpll_init(struct spll_multibackup_state *s);

void xpll_stop(struct spll_multibackup_state *s, int id_ref);

void xpll_start(struct spll_multibackup_state *s, int id_ref,
		      int id_out, int priority);

int xpll_update(struct spll_multibackup_state *s, int tag, int source);

int xpll_set_phase_shift(int channel, struct spll_multibackup_state *s, int desired_shift_ps);

void xpll_show_stats(struct spll_multibackup_state *s);

int xpll_get_first_backup(struct spll_multibackup_state *s);
#endif // __SPLL_MULTIBACKUP_H
