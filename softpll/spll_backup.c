/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2010 - 2014 CERN (www.cern.ch)
 * Author: Maciej Lipinski <maciej.lipinski@cern.ch> 
 *        Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

/* spll_backup.c - Implementation of the backup channel for DDMTD PLL. 
 * 
 * ------------------------------------------------------------------------------------------
 * Intro: my understanding of SoftPLL and its components
 * ------------------------------------------------------------------------------------------
 * The DDMTD clock is the one with slight offset to the reference, i.e.
 *   124.992371 MHz = [ 2^14/(2^14 + 1) ] *  125MHz or
 *    62.496185 MHz = [ 2^14/(2^14 + 1) ] * 62,5MHz
 * Mixing of DDMTD clock with ref/feedback clock results in an "offset clock" of
 * 7.629 kHz or 3.814 kHz respectively  
 * 
 * The "DDMTD clock" (125+MHz or 62.5+MHz) is used run a counter (TODO: verify this info) 
 * which tags the edge of the "offset clock".
 * The "DDMTD clock" is mixed with the following clocks to get the tags:
 * 1) feedback clock - the one that we control, which is local and which we use to encode
 *                     data on all the port
 * 2) active ref clk - the clock that is used as the reference for the feedback
 * 3) backup ref clks- the clocks that are backup
 * 
 * three usages of the tags:
 * 1) helper PLL controls "DDMTD clock". It  uses consecutive tags of the ref rx clock to 
 *    check whether it's frequency is "perfect". In other words, it uses the measurement 
 *    result (tags) to check the frequency of the clock (DDMTD clock) used for the measurement.
 *    The "offset clock" period should be (2^14) times the period of the "DDTMD clock" 
 *    (NOTE: the "offset clock" period should be (2^14+1) times the period of the 
 *    ref/feedback clock).
 *    So, consecutive tags should give perfect period (i.e. 2^14) of the the offset frequency.
 *    If this is not the case, the frequency of the "DDMTD clock" must be changed to give
 *    a correct measurement.
 *    This is how I understand it, ML 
 * 2) main PLL, controls the "feedback clock" and uses tags for two things:
 *    - control of frequency by checking whether the time advances in both clocks at the same
 *      pace, in other words the rising edge of both offset clocks, relative to the DDMTD counter
 *      (i.e. tag value), is tried to be maintained at the same value. In this way, the
 *      feedback clock follows the ref clock. 
 *    - once the feedback and ref are syntonized, the phase is adjusted:
 *      -> the adjustment is kept in the adder_ref incremented by 1 each SPLL updated,
 *         the adder does not hold the value of the tags, just the difference in phase and
 *         it is used to handle the overflow of tags
 *      -> the phase is adjusted by manipulating the frequency (DAC->VXCO), if we change the
 *         frequency, the clocks will advance differently and a phase difference will 
 *         be created. This phase difference is maintained thanks to the adder
 * 3) phase tracker uses the tags to measure the real phase offset 
 *    - it can measure phase offsets between the feedback clock and any other clock (rx ref, 
 *      aux)
 *    - in WRPTP synchronization, the phase measurement between feedback and rx ref is used
 *    - in nodes, the phase measurement between feedback and oux channels is used
 *    - in the backup switchover, the phase measurement between feedback and backup rx clock(s)
 *      is used
 *    - the phase measurement reflects averaged phase offset, the average is over n_avg
 *      previous samples every n_avg samples (no updates in between, it's not a moving
 *      average window, thus delayed updates of the value phase_val are expected)
 *
 *-------------------------------------------------------------------------------------------
 * Changes to the WR code-base (where/what)
 *-------------------------------------------------------------------------------------------
 *
 * The changes spam: PPSi, wrsw_hal and wrpc/softpll
 * PPSi (ppsi git repo, branch: ml-140906-switchover):
 * - added new port config: backup
 * - made the per-port priority atribute work - it is set to "1" if a port is "backup"
 * - modifed BMC to use prio/backup information
 * - enabled servo per port (servo-related info is now in a table)
 * - enabled currentDS per port (currentDS info is now in a table)
 * - modifed wr-servo.c to enable handling backup (many) servo, i.e. it uses prio to tell
 *   active from backup and uses the local static structure only for the active
 *   TODO: this is hackish - probably needs better way (this was already a mess...see comments)
 * - modified msg.c : this is a hack to prevent PTP messages received on 
 *   the backup port from being discarded, again, prio value used to tell active from backup
 *   TODO: good question how to make it nice...need some protocol hack (it seems)
 * - enabled adjust_phase() per port (was global) in wrs-time.c and servo.c
 * - added quite some debug
 * - TODO: change some hard-coded-size tables (currentDS, servos) to use global defines
 *         (candidate: PP_MAX_LINKS)
 * 
 * wrsw_hal (wr-switch-sw git repo, branch: ml-140906-switchover):
 * - enabled to pass priority value from PPSi to SoftPLL, when locking
 * - interpre the priority value to avoid reseting/etc SoftPLL when locking and link down
 * - remember and recognize backup channel/port number to use dedicated softPLL functions
 *   where needed
 * - added IPC call to SoftPLL: rts_backup_channel()
 * - enabled adjust_phase per port
 * - extended wr_phytool to enable setpoint adjustment
 * - added some debugs
 *
 * wrpc/softpll (wrpc-sw git repo, branch: ML-140901-switchover):
 * - added tones of debugs which are nasty but help to get an idea what is happening
 * - added IPC communication with wrsw_hal to handle backup
 * - added bakcup pll to handle backup port
 * - added functions to handle IPC calls to init/start/stop backp port
 * - added functions to switchover
 * 
 * 
 *-------------------------------------------------------------------------------------------
 * Backup switchover
 *-------------------------------------------------------------------------------------------
 * The switch over needs to mess up with the following parts of SoftPLL
 * 1) helper PLL - the source of the DDMTD clcok needs to be changed
 * 2) "main PLL" - what was done: a "backup PLL" was added which is a stripped down version
 *                 of the "main PLL". the aim of the backup PLL is to "fake" PLL functionalities
 *                 for the WRPTP, that includes: lock and timestamps enhancement. This enables
 *                 to obtain "operational data" that would be true if the backup PLL was
 *                 working as the main PLL. The operational data includes 
 *                 1) in softPLL: setpoint and phase value/offset measurement in the SoftPLL 
 *                 2) in WRPTP  : link delay and offset from master
 *                 The later (2) is possible since we run WRPTP exchange and "fake" WRPTP
 *                 synchronization on the backup port.
 *                 The fake operational data is needed to be able to take over the role of
 *                 the main PLL "at the full speed" (it's like changing a car driver while
 *                 driving 100km/h on the highway.
 * 
 * The above are described in details below
 * ------------------------------------------------------------------------------------------
 * Helper PLL switchover
 * ------------------------------------------------------------------------------------------
 * At the moment a "slow" switchover of helper PLL is implemented, it might not be sufficient.
 * How it works:
 * - the PI controller of the PLL works based on tags provided, i.e. it is tag-driven, updated
 *   each time new tag is received from HDL via FIFO+irq
 * - when the link goes down, there is no tag updates and the last DAC control word is 
 *   maintained. This is some kind of simple holdover.
 *   NOTE: The last control world might be somehow corrupted since the link never goes down 
 *   instantly. 
 *   TODO: So here some simple average might be required if the performance is not good enough
 * - the link down is detected in the wrsw_hal which polls link state and manages all ports
 * - when wrsw_hal detects "link down" on the link set to be backup, it commands SoftPLL to 
 *   switchover:
 *   wrsw_hal/hal_ports.c:handle_link_down()->rts_backup_channel(p->hw_index, RTS_BACKUP_CH_ACTIVATE);
 * - the switchover of helper PLL is done by (softpll/spll_helper.c:helper_switch_reference()): 
 *   1) switching off the tagger on the active rx clk, 
 *   2) "clearing the current tag-based measurement (setting p_adder=0, tag_d0-1 forces that)
 *   3) switching on the tagger on the port defined to be backup (active from now on)..
 *      TODO: hmm, this seems not necessary, to be verified 
 *   4) changing the ref_src value 
 * - after getting two consecutive tags, the PI sturts running again
 * - since the (previously) active rx ref clock and the (previously) backup rx ref clock are
 *   (supposed to be) the same and the frequency should not drift too much during the process,
 *   this should work.
 * - TODO: if it does not, two things can be done:
 *   1) implement primitive holdover or outlier elimination to discard the wrong tag while 
 *      disconnecting cable -> some kind of intelligence will need to be added here later
 *      probably, since the cable disconnection is a very theoretical failure use case
 *   2) provide information about active rx ref clock failure directly from HDL and activate
 *      the function based on that info (i.e. irq)
 *
 * ------------------------------------------------------------------------------------------
 * Backup PLL 
 * ------------------------------------------------------------------------------------------
 * It provides the facility to measure/track the phase shift between the feedback clock and 
 * the backup rx ref clock. Simirarily as main PLL, it allows to calculate the error between
 * the two clock, taking into account the setpoint, e.g.:
 * 
 * setpoint ~= phase measurement +/-jitter (due to frequency error)
 * 
 * it is represented by a special backup PLL structure (softpll/spll_backup.h): 
 * struct spll_backup_state bpll
 * which is derived from the main PLL. both sit in the softpll_state "global" structue
 * TODO: to enable more backup ports, bpll must be a table
 * 
 * In wrpc/softpll/softpll_ng.c I added added a bunch of spll_*_backup_* functions which 
 * "mirror" the spll_* functions but refer (update/read) to softpll->bpll rather than 
 * softpll->mpll
 * TODO: probably needs more beautiful solution later
 * 
 *-------------------------------------------------------------------------------------------
 * KNOWN problems/BUGs:
 * 1) the active port that is detected to go down is exptected to really go down, i.e.
 *    pre-hardware detection of down is expected to be correct and the real hardware down 
 *    happens. The case when pre-detection si false, is not handled.
 * 2) when fibers connected when starting up, the active/backu are wrongly recognized
 *    (i.e. the same becomes active and backup)
 */

#include "spll_backup.h"
#include "spll_debug.h"
#include "spll_ptracker.h"
#include <pp-printf.h>
#include "trace.h"
#include "spll_multibackup.h"

#define MPLL_TAG_WRAPAROUND 100000000

#define MATCH_NEXT_TAG 0
#define MATCH_WAIT_REF 1
#define MATCH_WAIT_OUT 2

#undef WITH_SEQUENCING

static inline void xpll_debug(struct spll_backup_state *s, int what, int value, int last)
{
	int bid = 3;
	if(s->xpll->bids[0] == s->id_ref) bid = 0; else
	if(s->xpll->bids[1] == s->id_ref) bid = 1; else
	if(s->xpll->bids[2] == s->id_ref) bid = 2; else
	if(s->xpll->bids[3] == s->id_ref) bid = 3; else return;
	spll_debug((what | ((0x3 & bid) << 4)), value, last);
}

/* initialization of pll "configuration" (unlike runtime data as in bpll_start())
 * copied from mpll except, just missing:
 * - the initialization of PI controller - no need, we don't control anything
 * - lock checkup - no need, we are not really locked 
 *   TODO: later, we might want to implement something like ld() but checking whether the
 *         active channel is ok with respect to backup(s), even voting logic (brrr) 
 */
void bpll_init(struct spll_backup_state *s)
{
	s->enabled = 0;

	/* Freqency branch lock detection */
	s->ld.threshold = 50;
	s->ld.lock_samples = 1000;
	s->ld.delock_samples = 100;
}

/*
 * this is to start bpll, it is alsomainly copy of mpll except:
 * - enabling of tagging on the feedback channel (id_out) as it is already in place 
 * - initializing PI/LD
 */
void bpll_start(struct spll_backup_state *s, int id_ref, int id_out, int priority)
{
	s->id_ref = id_ref;
	s->id_out = id_out;
	
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
	s->sample_n = 0;
	s->enabled = 1;
	s->holdover=0;
	s->stabilize_cntdown = 0;
	s->priority = priority;
	
	//IMPORTANT: the ration of short to long avg is an important parameter for 
	//           pre-detection (test-adjusted)
	avg_init((spll_avg_t *)&s->avg_err_short,3);
	avg_init((spll_avg_t *)&s->avg_err_long ,9);
		
	spll_enable_tagger(s->id_ref, 1);
	// start on all ports (this is for convenience when testing)
	spll_debug(DBG_EVENT | DBG_BACKUP, DBG_EVT_STARTBACKUP, 1);
	spll_debug(DBG_EVENT | DBG_BACKUP |(0x1<<4) , DBG_EVT_STARTBACKUP, 1);
	spll_debug(DBG_EVENT | DBG_BACKUP |(0x2<<4) , DBG_EVT_STARTBACKUP, 1);
	spll_debug(DBG_EVENT | DBG_BACKUP |(0x3<<4) , DBG_EVT_STARTBACKUP, 1);
	spll_debug(DBG_EVENT | DBG_MAIN, DBG_EVT_STARTBACKUP, 1);
	spll_debug(DBG_EVENT | DBG_HELPER, DBG_EVT_STARTBACKUP, 1);
}

void bpll_stop(struct spll_backup_state *s)
{
	spll_enable_tagger(s->id_ref, 0);
	bpll_clear(s);
}	
void bpll_clear(struct spll_backup_state *s)
{
	s->enabled             = 0;
	s->adder_ref           = 0;
	s->adder_out           = 0;
	s->tag_ref             = -1;
	s->tag_out             = -1;
	s->tag_ref_d           = -1;
	s->tag_out_d           = -1;
	s->seq_ref             = 0;
	s->phase_shift_target  = 0;
	s->phase_shift_current = 0;
	s->id_out              = -1;
	s->id_ref              = -1;
	s->err_d               = 0;
	s->holdover            = 0;
	s->ld.locked           = 0;
	s->priority            = -1;
}

/*
 * the main bulk of work is here. it is again taken from mpll, except:
 * - running the PI controller and then triving DAC
 * - verying whether we are locked on this channel - we don't check whether we are locked 
 *   on backup because:
 *   * in theory we do not need
 *   * in practice, at the beginning, the error (in the current state) is huge and it 
 *     indicates unlocked while we are really locked
 *   TODO: later, this function (ld_update) could be actually used to check whether the
 *         two cloks (active and backup(s)) do not wander with respect to each other
 * 
 * some additional magic is considered here (see the code below)
 * 
 */
int bpll_update(struct spll_backup_state *s, int tag, int source)
{
	if(!s->enabled)
	    return SPLL_LOCKED;

// 	int hw_status = spll_channel_status(s->id_ref);
// 	if(s->hw_status_d && !hw_status) // link went down
// 	{
// 	    s->hw_status_d = hw_status;
// 	    return SPLL_DOWN;
// 	}
// 	s->hw_status_d = hw_status;
	
	int err = 0;
	int en;
	int32_t phase=0;

	if (source == s->id_ref)
		s->tag_ref = tag;
	else if (source == s->id_out)
		s->tag_out = tag;
	else
		return SPLL_LOCKED;

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
// 		if (s->ld.locked) 
// 		{
			err &= (1 << HPLL_N) - 1;
			if (err & (1 << (HPLL_N - 1)))
				err |= ~((1 << HPLL_N) - 1);
// 		}

#endif
		if(s->holdover==0)
		    s->err_d = err;
		avg_update((spll_avg_t *)&s->avg_err_short, err);
		avg_update((spll_avg_t *)&s->avg_err_long,  err);

// 		spll_debug(DBG_BACKUP | DBG_TAG, s->tag_out + s->adder_out, 0);
// 		spll_debug(DBG_BACKUP | DBG_REF, s->err_d, 0);
// 		spll_debug(DBG_BACKUP | DBG_AVG_L, avg_get((spll_avg_t *)&s->avg_err_long, AVG_HIST_RECENT), 0);
// 		spll_debug(DBG_BACKUP | DBG_AVG_S, avg_get((spll_avg_t *)&s->avg_err_short, AVG_HIST_RECENT), 0);
// 		spll_debug(DBG_BACKUP | DBG_ERR, err, 0);
// 		spll_debug(DBG_BACKUP | DBG_SAMPLE_ID, s->sample_n++, 1);

		xpll_debug(s, DBG_BACKUP | DBG_TAG, s->tag_out + s->adder_out, 0);
		xpll_debug(s, DBG_BACKUP | DBG_REF, s->err_d, 0);
		xpll_debug(s, DBG_BACKUP | DBG_AVG_L, avg_get((spll_avg_t *)&s->avg_err_long, AVG_HIST_RECENT), 0);
		xpll_debug(s, DBG_BACKUP | DBG_AVG_S, avg_get((spll_avg_t *)&s->avg_err_short, AVG_HIST_RECENT), 0);
		xpll_debug(s, DBG_BACKUP | DBG_ERR, err, 0);
		xpll_debug(s, DBG_BACKUP | DBG_SAMPLE_ID, s->sample_n++, 1);
		
		
		s->tag_out = -1;
		s->tag_ref = -1;

		if (s->adder_ref > 2 * MPLL_TAG_WRAPAROUND
		    && s->adder_out > 2 * MPLL_TAG_WRAPAROUND) {
			s->adder_ref -= MPLL_TAG_WRAPAROUND;
			s->adder_out -= MPLL_TAG_WRAPAROUND;
		}
		
		// just set the proper setpoint, this is completly faked so nothing will happen
		s->adder_ref          += (s->phase_shift_target - s->phase_shift_current);
		s->phase_shift_current = s->phase_shift_target;
		
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

/*
 * all the functions below are copied from mpll, 
 * TODO: put it to spll_common.c ? or some shared place 
 */
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

int bpll_set_phase_shift(struct spll_backup_state *s, int desired_shift_ps)
{
	int div = (DIVIDE_DMTD_CLOCKS_BY_2 ? 2 : 1);
	s->phase_shift_target = from_picos(desired_shift_ps) / div;
// 	TRACE_DEV("[bpll] set target phaseshift %d\n", s->phase_shift_target);
	return 0;
}

int bpll_shifter_busy(struct spll_backup_state *s)
{
	return s->phase_shift_target != s->phase_shift_current;
}
void bpll_show_stats(struct spll_backup_state *s)
{
  
	
	TRACE_DEV("| bPLL@ %2d bL-%s, bErr:%6d",
		 s->id_ref,
		(s->ld.locked ? "yes":" no"),
		 s->err_d);

// 	avg_dump((spll_avg_t *)&softpll.bpll.avg_err_long, "b_err_l");
// 	avg_dump((spll_avg_t *)&softpll.bpll.avg_err_short,"b_err_s");
}

int bpll_avg_check(struct spll_backup_state *s, int *bs_avg_l, int *bs_avg_s)
{
	if(s->ld.locked == 0)
	{
		s->stabilize_cntdown = 0x1<<13;
		return -1;
	}
	else if(s->stabilize_cntdown > 0)
	{
		s->stabilize_cntdown--;
		return -1;
	}
	else
	{
		if(s->stabilize_cntdown == 0)
		{
			s->stabilize_cntdown--;
			TRACE_DEV("backup %d  stabilized, start pre-down detection\n",s->id_ref);
		}
		*bs_avg_l = avg_get((spll_avg_t *)&s->avg_err_long , AVG_HIST_RECENT);
		*bs_avg_s = avg_get((spll_avg_t *)&s->avg_err_short, AVG_HIST_RECENT);
		return abs(*bs_avg_l - *bs_avg_s);
	}
}