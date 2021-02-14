/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <wrc.h>

#include "board.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include "softpll_ng.h"
#include "wrc_ptp.h"
#include "dev/storage.h"
#include "ptpd_netif.h"
#include "dev/rxts_calibrator.h"

/* New calibrator for the transition phase value. A major pain in the ass for
   the folks who frequently rebuild their gatewares. The idea is described
   below:
   - lock the PLL to the master
   - scan the whole phase shifter range
   - at each scanning step, generate a fake RX timestamp.
   - check if the rising edge counter is ahead of the falling edge counter
     (added a special bit for it in the TSU).
   - determine phases at which positive/negative transitions occur
   - transition phase value is in the middle between the rising and falling
     edges.
   
   This calibration procedure is fast enough to be run on slave nodes whenever
   the link goes up. For master mode, the core must be run at least once as a
   slave to calibrate itself and store the current transition phase value in
   the EEPROM.
*/

/* how finely we scan the phase shift range to determine where we have the bit
 * flip */
#define CAL_SCAN_STEP 100

/* deglitcher threshold (to remove 1->0->1 flip bit glitches that might occur
   due to jitter) */
#define CAL_DEGLITCH_THRESHOLD 5

/* we scan at least one clock period to look for rising->falling edge transition
   plus some headroom */
#define CAL_SCAN_RANGE (REF_CLOCK_PERIOD_PS + \
		(3 * CAL_DEGLITCH_THRESHOLD * CAL_SCAN_STEP))

#define TD_WAIT_INACTIVE	0
#define TD_GOT_TRANSITION	1
#define TD_DONE			2

/* Number of retries for rxts_calibration_update
 * value found experimentally */
#define CALIB_RETRIES 1000


/* state of transition detector */
struct trans_detect_state {
	int prev_val;
	int sample_count;
	int state;
	int trans_phase;
};

/* finds the transition in the value of flip_bit and returns phase associated
   with it. If no transition phase has been found yet, returns 0. Non-zero
   polarity means we are looking for positive transitions, 0 - negative
   transitions */
static int lookup_transition(struct trans_detect_state *state, int flip_bit,
			     int phase, int polarity)
{
	if (polarity)
		polarity = 1;

	switch (state->state) {
	case TD_WAIT_INACTIVE:
		/* first, wait until we have at least CAL_DEGLITCH_THRESHOLD of
		   inactive state samples */
		if (flip_bit != polarity)
			state->sample_count++;
		else
			state->sample_count = 0;

		if (state->sample_count >= CAL_DEGLITCH_THRESHOLD) {
			state->state = TD_GOT_TRANSITION;
			state->sample_count = 0;
		}

		break;

	case TD_GOT_TRANSITION:
		if (flip_bit != polarity)
			state->sample_count = 0;
		else {
			state->sample_count++;
			if (state->sample_count >= CAL_DEGLITCH_THRESHOLD) {
				state->state = TD_DONE;
				state->trans_phase =
				    phase -
				    CAL_DEGLITCH_THRESHOLD * CAL_SCAN_STEP;
			}
		}
		break;

	case TD_DONE:
		return 1;
		break;
	}
	return 0;
}

static struct trans_detect_state det_rising[wr_num_ports], det_falling[wr_num_ports];
static int cal_cur_phase[wr_num_ports];

/* Starts RX timestamper calibration process state machine. Invoked by
   ptpnetif's check lock function when the PLL has already locked, to avoid
   complicating the API of ptp-noposix/ppsi. */

void rxts_calibration_start(uint8_t port)
{
	cal_cur_phase[port] = 0;
	det_rising[port].prev_val = det_falling[port].prev_val = -1;
	det_rising[port].state = det_falling[port].state = TD_WAIT_INACTIVE;
	det_rising[port].sample_count = 0;
	det_falling[port].sample_count = 0;
	det_rising[port].trans_phase = 0;
	det_falling[port].trans_phase = 0;
	spll_set_phase_shift(0, 0);
}

/* Updates RX timestamper state machine. Non-zero return value means that
   calibration is done. */
int rxts_calibration_update(uint32_t *t24p_value, int port)
{
	int32_t ttrans = 0;

	if (spll_shifter_busy(0))
		return 0;

	/* generate a fake RX timestamp and check if falling edge counter is
	   ahead of rising edge counter */
	int flip = ep_timestamper_cal_pulse(port);

	/* look for transitions (with deglitching) */
	lookup_transition(&det_rising[port], flip, cal_cur_phase[port], 1);
	lookup_transition(&det_falling[port], flip, cal_cur_phase[port], 0);

	if (cal_cur_phase[port] >= CAL_SCAN_RANGE) {
		if (det_rising[port].state != TD_DONE || det_falling[port].state != TD_DONE) 
		{
			wrc_verbose("RXTS calibration error.\n");
			return -1;
		}

		/* normalize */
		while (det_falling[port].trans_phase >= REF_CLOCK_PERIOD_PS)
			det_falling[port].trans_phase -= REF_CLOCK_PERIOD_PS;
		while (det_rising[port].trans_phase >= REF_CLOCK_PERIOD_PS)
			det_rising[port].trans_phase -= REF_CLOCK_PERIOD_PS;

		/* Use falling edge as second sample of rising edge */
		if (det_falling[port].trans_phase > det_rising[port].trans_phase)
			ttrans = det_falling[port].trans_phase - REF_CLOCK_PERIOD_PS/2;
		else if(det_falling[port].trans_phase < det_rising[port].trans_phase)
			ttrans = det_falling[port].trans_phase + REF_CLOCK_PERIOD_PS/2;
		ttrans += det_rising[port].trans_phase;
		ttrans /= 2;

		/*normalize ttrans*/
		if(ttrans < 0) ttrans += REF_CLOCK_PERIOD_PS;
		if(ttrans >= REF_CLOCK_PERIOD_PS) ttrans -= REF_CLOCK_PERIOD_PS;


		wrc_verbose("RXTS calibration: R@%dps, F@%dps, transition@%dps\n",
			  det_rising[port].trans_phase, det_falling[port].trans_phase,
			  ttrans);

		*t24p_value = (uint32_t)ttrans;
		return 1;
	}

	cal_cur_phase[port] += CAL_SCAN_STEP;

	spll_set_phase_shift(0, cal_cur_phase[port]);
	return 0;
}

/* legacy function for 'calibration force' command */
int measure_t24p(uint32_t *value, int port)
{
	int rv;
	int retry_cnt;
	pp_printf("Waiting for link...\n");
	while (!ep_link_up(NULL, port))
		timer_delay_ms(100);

  // spll contains rx/tx clocks
	spll_init(SPLL_MODE_SLAVE, 2*port, 1);
	pp_printf("Locking PLL...\n");
	while (!spll_check_lock(0))
	{
		timer_delay_ms(100);
		retry_cnt++;
		if (retry_cnt>400)
			return -1;
	}
	pp_printf("\n");

	pp_printf("Calibrating RX timestamper...\n");
	rxts_calibration_start(port);

	while (!(rv = rxts_calibration_update(value,port))) ;
	return rv;
}

/* Delays for master must have been calibrated while running as slave */
static int calib_t24p_master(uint32_t *value, int port)
{
	int rv;

	rv = storage_phtrans(value, 0, port);
	if(rv < 0) {
		pp_printf("port %d Error %d while reading t24p from storage\n", port, rv);
		return rv;
	}
	pp_printf("port %d t24p read from storage: %d ps\n", port,*value);
	return rv;
}


/*SoftPLL must be locked prior calling this function*/
static int calib_t24p_slave(uint32_t *value, int port)
{
	int rv;
	uint32_t prev;
	int retries = 0;

	while (!(rv = rxts_calibration_update(value,port))) {
		if (retries > CALIB_RETRIES || ep_link_up(NULL, port) == LINK_DOWN)
			return -1;
 		retries++;
	}
	if (rv < 0) {
		/* Fall back on master == eeprom-or-error */
		return calib_t24p_master(value,port);
	}

	/*
	 * Let's see if we have a matching value in EEPROM:
	 * accept a 200ps difference, otherwise rewrite eeprom
	 */
	rv = storage_phtrans(&prev, 0 /* rd */, port);
	if (rv < 0 || (prev < *value - 200) || (prev > *value + 200)) {
		rv = storage_phtrans(value, 1, port);
		pp_printf("Wrote new t24p value: %d ps (%s)\n", *value,
			  rv < 0 ? "Failed" : "Success");
	}
	return 0;
}

int calib_t24p(int mode, uint32_t *value, int port)
{
	int ret;

	if (mode == WRC_MODE_SLAVE)
		ret = calib_t24p_slave(value, port);
	else
		ret = calib_t24p_master(value, port);

	//update phtrans value in socket struct
	if (ret >= 0)
		ptpd_netif_set_phase_transition(*value, port);
	return ret;
}
