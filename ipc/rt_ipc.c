/*
 * Mini-ipc: an example freestanding server, based in memory
 *
 * Copyright (C) 2011,2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * This code is copied from trivial-server, and made even more trivial
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <wrc.h>

#include "minipc.h"

#define RTIPC_EXPORT_STRUCTURES
#include "rt_ipc.h"

#include <softpll_ng.h>

static struct rts_pll_state pstate;

static void clear_state()
{
	int i;
	for(i=0;i<RTS_PLL_CHANNELS;i++)
	{
	    pstate.channels[i].priority = 0;
	    pstate.channels[i].phase_setpoint = 0;
	    pstate.channels[i].phase_loopback = 0;
	    pstate.channels[i].flags = CHAN_REF_VALID;
    }
    pstate.flags = 0;
    pstate.current_ref = -1;//TODO[?]: init with -1 to make sure it does not much channel 0
    pstate.backup_ref = -1; //TODO[?]: init with -1 to make sure it does not much channel 0
    pstate.old_ref = -1; //TODO[?]: init with -1 to make sure it does not much channel 0
    pstate.mode = RTS_MODE_DISABLED;
    pstate.ipc_count = 0;
    pstate.switchover_ocured = 0;
}
static void clear_switchover_occured()
{
    pstate.switchover_ocured = 0;
//     TRACE("Cleared switch over occured\n");
}
static void set_switchover_occured()
{
    pstate.switchover_ocured = 1;
//     TRACE("Set switch over occured\n");
}
static void set_backup_channel(int channel)
{
    pstate.backup_ref = channel;
}
/* Sets the phase setpoint on a given channel */
int rts_adjust_phase(int channel, int32_t phase_setpoint)
{
    TRACE("Adjusting phase: ref channel %d, setpoint=%d ps [switchover occured=%d,"
    " loopback_phase=%d]\n", channel, phase_setpoint, pstate.switchover_ocured,
    (int)pstate.channels[channel].phase_loopback);
    if(pstate.switchover_ocured == 1 && pstate.current_ref == channel)
    {
	clear_switchover_occured();
	return (int)pstate.channels[channel].phase_good_val;
    }
    if(pstate.current_ref == channel)
         spll_set_phase_shift(0, phase_setpoint);
    else if(pstate.backup_ref == channel)
	  spll_set_backup_phase_shift(phase_setpoint);
//     {
// 	 /* This is kind of a hack:
// 	  * The Offset from master (OFM) is calculated as: 
// 	  * offset = t1-(t2-phase) + delayMM/2
// 	  * delayMM= [t4-t1] - [t3-(t2-phase)]
// 	  * so
// 	  * offset = t1-t2 + [(t4-t1)-t3-t2)] + phase - phase/2
// 	  * so, if we don't enhance the timestamp with the phase measurement, the error
// 	  * of the calculated OFM should be:
// 	  * error = phase - phase/2 = phase/2
// 	  * this error is provided as a setpoint. for the backup port, the setpoint is made
// 	  * equal to the phase measurement, so we get phase/2 correction instead of phase.
// 	  * therefore, we multiply, the measured phase error of the OFM by 2
// 	  * (really not sure it wil work)
// 	  */ 
//       phase_setpoint = phase_setpoint << 1;
//       spll_set_backup_phase_shift(phase_setpoint); 
//     }

    
    pstate.channels[channel].phase_setpoint = phase_setpoint;

    return 0;
}

/* Sets the RT subsystem mode (Boundary Clock or Grandmaster) */
int rts_set_mode(int mode)
{
	int i;

	const struct {
		int mode_rt;
		int mode_spll;
		int do_init;
		char *desc;
	} options[] = {
		{ RTS_MODE_GM_EXTERNAL, SPLL_MODE_GRAND_MASTER, 1, "Grand Master (external clock)" },
		{ RTS_MODE_GM_FREERUNNING, SPLL_MODE_FREE_RUNNING_MASTER, 1, "Grand Master (free-running clock)" },
		{ RTS_MODE_BC, SPLL_MODE_SLAVE, 0, "Boundary Clock (slave)" },
		{ RTS_MODE_DISABLED, SPLL_MODE_DISABLED, 1, "PLL disabled" },
		{ RTS_MODE_BC_BACKUP, SPLL_MODE_BACKUP_SLAVE, 2, "Activate backup" }, //not used for the time being, separat functin to manage backup port
		{ 0,0,0, NULL }
	};

	pstate.mode = mode;

	for(i=0;options[i].desc != NULL;i++)
		if(mode == options[i].mode_rt)
		{
			TRACE("RT: Setting mode to %s.\n", options[i].desc);
			if(options[i].do_init > 1)
				TRACE("RT: switchover here.\n");
			else if(options[i].do_init)
				spll_init(options[i].mode_spll, 0, 1);
			else
				spll_init(SPLL_MODE_DISABLED, 0, 0);
		}

	return 0;
}

/* Manage the backup channel, called by wrsw_hal via IPC */
int rts_backup_channel(int channel, int cmd)
{
	switch(cmd)
	{
		/* activate backup and remember on which port it is*/
		case RTS_BACKUP_CH_LOCK:
		pstate.switchover_ocured = 0;
		set_backup_channel(channel);
		spll_start_backup(channel);
		TRACE("RT [backup port]: locked !!! : %d \n", channel);
		break;
		case RTS_BACKUP_CH_ACTIVATE:
		pstate.switchover_ocured = 1;
		TRACE("switchover detected by wrsw_hall, finally but we are already done\n");
// 		spll_switchover(pstate.backup_ref);
// 		pstate.current_ref = pstate.backup_ref;
		set_backup_channel(-1);
		pstate.old_ref    = -1; 
		
		
		TRACE("RT [backup port]: activated !!! : %d \n", channel);
		break;
		case RTS_BACKUP_CH_DOWN:
		spll_stop_backup(channel);	
		set_backup_channel(-1);
		TRACE("RT [backup port]: down !!! : %d \n", channel);
		break;
	}

	return 0;
}

/* Reference channel configuration (BC mode only) */
int rts_lock_channel(int channel, int priority)
{
	if(pstate.mode != RTS_MODE_BC)
	{
        TRACE("trying to lock while not in slave mode,..\n");
		return -1;
    }


	TRACE("RT [slave]: Locking to: %d (prio %d)\n", channel, priority);
	pstate.channels[channel].priority = priority;
	if(priority == 0) 
	{
		spll_init(SPLL_MODE_SLAVE, channel, 0);
		pstate.current_ref = channel;
	}
	else
	{
		rts_backup_channel(channel, RTS_BACKUP_CH_LOCK);
	}
    

	return 0;
}

void rts_init(void)
{
    clear_state();
}

void rts_update(void)
{
    int i;
    int n_ref;
    int enabled;
		
    spll_get_num_channels(&n_ref, NULL);
    if(pstate.backup_ref != -1 && spll_check_switchover(pstate.current_ref,pstate.backup_ref) == 1 )
    {
		pstate.current_ref = pstate.backup_ref;
		pstate.old_ref     = pstate.backup_ref;
		pstate.backup_ref  = -1;
		set_switchover_occured();
    }

    pstate.flags = (spll_check_lock(0) ? RTS_DMTD_LOCKED | RTS_REF_LOCKED : 0);
    for(i=0;i<RTS_PLL_CHANNELS;i++)
    {
#define CH pstate.channels[i]
        CH.flags = 0;
        CH.phase_loopback = 0;
        CH.phase_current = 0;
//        CH.phase_setpoint = 0;
        CH.phase_loopback = 0;
        CH.phase_good_val = 0;

        if(i >= n_ref)
            CH.flags = CHAN_DISABLED;
        else {
            if(i==pstate.current_ref)
            {
                spll_get_phase_shift(0, &CH.phase_current, NULL,  &CH.phase_good_val);
		            if(spll_shifter_busy(0))
		            	CH.flags |= CHAN_SHIFTING;
						}
            else if(i==pstate.backup_ref)
            {
                spll_get_backup_phase_shift(&CH.phase_current, NULL, &CH.phase_good_val);
// 		            if(spll_shifter_busy(0))
// 		            	CH.flags |= CHAN_SHIFTING;
						}

            if(spll_read_ptracker(i, &CH.phase_loopback, &enabled))
	            CH.flags |= CHAN_PMEAS_READY;
// 	          
	          CH.flags |= (enabled ? CHAN_PTRACKER_ENABLED : 0);

        }

#undef CH
    }
//     show_info();
}


/* fixme: this assumes the host is BE */
static int htonl(int i)
{
    return i;
}


static int rts_get_state_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
    struct rts_pll_state *tmp = (struct rts_pll_state *)ret;
    int i;

		pstate.ipc_count++;

    /* gaaaah, somebody should write a SWIG plugin for generating this stuff. */
    tmp->current_ref = htonl(pstate.current_ref);
    tmp->backup_ref = htonl(pstate.backup_ref);
    tmp->flags = htonl(pstate.flags);
    tmp->holdover_duration = htonl(pstate.holdover_duration);
    tmp->mode = htonl(pstate.mode);
		tmp->delock_count = spll_get_delock_count();
		tmp->ipc_count = pstate.ipc_count;
		
    for(i=0; i<RTS_PLL_CHANNELS;i++)
    {
        tmp->channels[i].priority = htonl(pstate.channels[i].priority);
        tmp->channels[i].phase_setpoint = htonl(pstate.channels[i].phase_setpoint);
        tmp->channels[i].phase_current = htonl(pstate.channels[i].phase_current);
        tmp->channels[i].phase_loopback = htonl(pstate.channels[i].phase_loopback);
        tmp->channels[i].phase_good_val = htonl(pstate.channels[i].phase_good_val);
        tmp->channels[i].flags = htonl(pstate.channels[i].flags);
//         if(tmp->channels[i].flags & CHAN_PTRACKER_ENABLED)
//            TRACE("RT [chan: %d] setpoint: %d, loopback real: %d [cor:%d], prio: %d, cur: %d\n", 
//         i, tmp->channels[i].phase_setpoint, htonl(pstate.channels[i].phase_loopback),
//         tmp->channels[i].phase_loopback, tmp->channels[i].priority, 
//         tmp->channels[i].phase_current);
    }

    return 0;
}

static int rts_set_mode_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
    *(int *) ret = rts_set_mode(args[0]);
    return 0;
}


static int rts_lock_channel_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
    *(int *) ret = rts_lock_channel(args[0], (int)args[1]);
    return 0;
}

static int rts_adjust_phase_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
    *(int *) ret = rts_adjust_phase((int)args[0], (int)args[1]);
    return 0;
}

static int rts_enable_ptracker_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
		spll_enable_ptracker((int)args[0], (int)args[1]);
    *(int *) ret = 0;
    return 0;
}

static int rts_debug_command_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
    *(int *) ret = rts_debug_command((int)args[0], (int)args[1]);
    return 0;
}

static int rts_backup_channel_func(const struct minipc_pd *pd, uint32_t *args, void *ret)
{
		pstate.ipc_count++;
    *(int *) ret = rts_backup_channel((int)args[0], (int)args[1]);
    return 0;
}

static struct minipc_ch *server;

int rtipc_init(void)
{
	/* The mailbox is mapped at 0x7000 in the linker script */
// 	server = minipc_server_create("mem:E000", 0);
// 	server = minipc_server_create("mem:7000", 0);
	server = minipc_server_create("mem:F000", 0);
	if (!server)
		return 1;

	rtipc_rts_set_mode_struct.f = rts_set_mode_func;
	rtipc_rts_get_state_struct.f = rts_get_state_func;
	rtipc_rts_lock_channel_struct.f = rts_lock_channel_func;
	rtipc_rts_adjust_phase_struct.f = rts_adjust_phase_func;
	rtipc_rts_enable_ptracker_struct.f = rts_enable_ptracker_func;
	rtipc_rts_debug_command_struct.f = rts_debug_command_func;
	rtipc_rts_backup_channel_struct.f = rts_backup_channel_func;
	
	minipc_export(server, &rtipc_rts_set_mode_struct);
	minipc_export(server, &rtipc_rts_get_state_struct);
	minipc_export(server, &rtipc_rts_lock_channel_struct);
  minipc_export(server, &rtipc_rts_adjust_phase_struct);
  minipc_export(server, &rtipc_rts_enable_ptracker_struct);
  minipc_export(server, &rtipc_rts_debug_command_struct);
  minipc_export(server, &rtipc_rts_backup_channel_struct);


	return 0;
}


void rtipc_action(void)
{
		minipc_server_action(server, 1000);
}
