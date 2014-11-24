/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2014 CERN (www.cern.ch)
 * Author: Maciej Lipinski <maciej.lipinski@cern.ch> 
 *        Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_multibackup.c - wrapper around spll_backup.c stuff to make it multiport
 * 
 * 
 */

#include "spll_backup.h"
#include "spll_multibackup.h"
#include "spll_debug.h"
#include <pp-printf.h>
#include "trace.h"



void xpll_init(struct spll_multibackup_state *s)
{
	int i;
	//init all backups
	for (i=0; i < BACKUP_ENTRIES_NUM; i++)
		bpll_init(&s->bpll[i]);

	//clear table of backups
	for (i=0; i < BACKUP_ENTRIES_NUM; i++)
		s->bids[i] = BACKUP_EMPTY_ID; // always empty/disabled backup
	
	s->backup_number = 0;
}

void xpll_start(struct spll_multibackup_state *s,int id_ref, int id_out, int priority)
{
	int new_id = -1,i, bid;
	if(s->bpll[s->bids[0]].enabled == 0) // no backup ports yet
		new_id = 0;
	else
	{
		for (i = (BACKUP_ENTRIES_NUM-2); i >= 0;i++)
		{
			bid = s->bids[i]; 
			if(s->bpll[bid].enabled == 0 && s->bpll[bid].priority > priority)
			{
				s->bids[i+1] = bid; // move
			}
			else
			{
				new_id = i+1;
				break;
			}
		}
	}
	if(new_id < 0)
	{
		//TODO: handle this exception somehow
		TRACE("MultiBackup->ERROR[xpll_start()]: no more place at this prio\n");
		return;
	}
	s->bids[new_id] = id_ref;
	bpll_start(&s->bpll[id_ref],id_ref, id_out, priority);
	s->backup_number++;
	TRACE("[xpll_start()] added backup: bids[%d]=%d |  backup_number=%d | prio %d\n",
	new_id, id_ref, s->backup_number,priority);
	
}

void xpll_stop(struct spll_multibackup_state *s, int id_ref)
{
	bpll_stop(&s->bpll[id_ref]);
	if(s->backup_number > 0) //sanity check
		s->backup_number--;
	else
		TRACE("MultiBackup->ERROR[xpll_stop()]: to many backup ports stopped\n");
}

int xpll_update(struct spll_multibackup_state *s, int tag, int source)
{
	int i, bid, bpll_cnt = 0;
	if(!s->backup_number) 
	    return SPLL_LOCKED; // no backup ports, desolee
		
	for (i=0;i<BACKUP_ENTRIES_NUM;i++)
	{
		bid = s->bids[i]; 
		if(s->bpll[bid].enabled == 0 || bpll_cnt > s->backup_number) 
			return SPLL_LOCKED; // no more backup ports 
		
		bpll_update(&s->bpll[bid], tag, source); // update
		bpll_cnt++;
	}
	return SPLL_LOCKING;
}

int xpll_set_phase_shift(int channel, struct spll_multibackup_state *s, int desired_shift_ps)
{
	bpll_set_phase_shift(&s->bpll[channel], desired_shift_ps);
	return 0;
}

void xpll_show_stats(struct spll_multibackup_state *s)
{
	int i, bid, bpll_cnt = 0;
	if(!s->backup_number) 
	{
	    TRACE_DEV("| no backup timing");
	    return;
	}

	for (i=0;i<BACKUP_PRIO_PORT_NUM;i++)
	{
		bid = s->bids[i]; 
		if(s->bpll[bid].enabled == 0) 
			break; // no more backup ports at this priority
		
		TRACE_DEV("| %d: ", (bpll_cnt+1) );
		bpll_show_stats(&s->bpll[bid]);
		bpll_cnt++;
	}
}

int xpll_get_first_backup(struct spll_multibackup_state *s)
{
	if(!s->backup_number) 
	    return -1;

	if(s->bpll[s->bids[0]].enabled == 1) 
		return s->bids[0];
	return -1;
}