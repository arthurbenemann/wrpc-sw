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
	int i,j;
	//init all backups
	for (i=0; i < BACKUP_ENTRIES_NUM; i++)
		bpll_init(&s->bpll[i]);

	//clear table of backups
	for (i=0; i < BACKUP_PRIO_NUM; i++)
		for (j=0; j < BACKUP_PRIO_PORT_NUM; j++)
			s->bids[i][j] = BACKUP_EMPTY_ID; // always empty/disabled backup
	
	s->backup_number = 0;
}

void xpll_start(struct spll_multibackup_state *s,int id_ref, int id_out, int priority)
{
	int new_id = -1,j, bid, bpll_cnt = 0;
	for (j=0;j<BACKUP_PRIO_PORT_NUM;j++)
	{
		bid = s->bids[priority][j]; 
		if(s->bpll[bid].enabled == 0)
		{
			new_id = j;
			break;
		}
	}
	if(new_id<0)
	{
		//TODO: handle this exception somehow
		TRACE("MultiBackup->ERROR[xpll_start()]: no more place at this prio\n");
		return;
	}
	s->bids[priority][new_id] = id_ref;
	bpll_start(&s->bpll[id_ref],id_ref, id_out, priority);
	s->backup_number++;
	TRACE("[xpll_start()] added backup: bids[%d][%d]=%d |  backup_number=%d\n",
	priority,new_id, id_ref, s->backup_number);
	
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
	int i,j, bid, bpll_cnt = 0;
	if(!s->backup_number) 
	    return SPLL_LOCKED; // no backup ports, desolee

	for (i=0;i<BACKUP_PRIO_NUM;i++)
	{
		if(bpll_cnt == s->backup_number) 
			return SPLL_LOCKED; // all backup ports checked
		
		for (j=0;j<BACKUP_PRIO_PORT_NUM;j++)
		{
			bid = s->bids[i][j]; 
			if(s->bpll[bid].enabled == 0) 
				break; // no more backup ports at this priority
			
			bpll_update(&s->bpll[bid], tag, source); // update
			bpll_cnt++;
		}
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
	int i,j, bid, bpll_cnt = 0;
	if(!s->backup_number) 
	{
	    TRACE_DEV("| no backup timing");
	    return;
	}

	for (i=0;i<BACKUP_PRIO_NUM;i++)
	{
		if(bpll_cnt == s->backup_number) 
		{
// 			TRACE_DEV("\n");
			return;
		}
		
		for (j=0;j<BACKUP_PRIO_PORT_NUM;j++)
		{
			bid = s->bids[i][j]; 
			if(s->bpll[bid].enabled == 0) 
				break; // no more backup ports at this priority
			
			TRACE_DEV("| %d: ", (bpll_cnt+1) );
			bpll_show_stats(&s->bpll[bid]);
			bpll_cnt++;
		}
	}	
}

int xpll_get_first_backup(struct spll_multibackup_state *s)
{
	int i, bid;
	if(!s->backup_number) 
	    return -1;

	for (i=0;i<BACKUP_PRIO_NUM;i++)
	{
		bid = s->bids[i][0]; 
		if(s->bpll[bid].enabled == 1) 
			return s->bids[i][0];
	}	
	return -1;
}