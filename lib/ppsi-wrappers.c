/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 - 2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <libwr/hal_shmem.h>
#include <libwr/shmem.h>
#include <wrc_ptp.h>
#include <dev/syscon.h>
#include <dev/endpoint.h>
#include <softpll_ng.h>
#include <ptpd_netif.h>

struct wrs_shm_head *ppsi_head;

/* Following code from ptp-noposix/libposix/freestanding-wrapper.c */

static int read_phase_val(struct hal_port_state *port, int ep_port)
{
	int32_t dmtd_phase;

	if (spll_read_ptracker(2*ep_port, &dmtd_phase, NULL)) {
		port->phase_rx_val = dmtd_phase;
		port->phase_rx_val_valid = 1;
	} else {
		port->phase_rx_val = 0;
		port->phase_rx_val_valid = 0;
	}

	if (spll_read_ptracker(2*ep_port+1, &dmtd_phase, NULL))
	{
		port->phase_tx_val = dmtd_phase;
		port->phase_tx_val_valid = 1;
	}
	else
	{
		port->phase_tx_val = 0;
		port->phase_tx_val_valid = 0;
	}

	return 0;
}

extern uint32_t cal_phase_transition[2];
extern int32_t sfp_alpha[2];

int wrpc_get_port_state(struct hal_port_state *state, const char *port_name)
{
	int port = atoi(&port_name[2]);
	int wrc_mode = wrc_ptp_get_mode(port);

	if(wrc_mode == WRC_MODE_SLAVE)
		state->mode = HEXP_PORT_MODE_WR_SLAVE;
	else
		state->mode = HEXP_PORT_MODE_WR_MASTER;

	/* all deltas are added anyway */
	ep_get_deltas(&state->calib.delta_tx_board, &state->calib.delta_rx_board, port);
	state->calib.delta_tx_phy = 0;
	state->calib.delta_rx_phy = 0;
	state->calib.sfp.delta_tx_ps = 0;
	state->calib.sfp.delta_rx_ps = 0;
	read_phase_val(state,port);
	state->state = ep_link_up(NULL,port);
	state->calib.tx_calibrated = 1;
	state->calib.rx_calibrated = 1;
	state->locked = spll_check_lock(0);
	state->clock_period  = REF_CLOCK_PERIOD_PS;
	state->t2_phase_transition = cal_phase_transition[port];
	state->t4_phase_transition = cal_phase_transition[port];
	get_mac_addr(state->hw_addr, port);
	state->hw_index = 0;	

	return 0;
}

/* dummy function, no shmem locks (no even shmem) are implemented in wrpc */
void wrs_shm_write(void *headptr, int flags)
{
	return;
}
