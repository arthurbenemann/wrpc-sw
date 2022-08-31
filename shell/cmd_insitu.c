/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2022 CERN (www.cern.ch)
 * Author: Adam Wujek <dev_public@wujek.eu>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "shell.h"
#include "dev/endpoint.h"
#include <string.h>
#include <wrc.h>
#include <errno.h>
#include <ppsi/ppsi.h>
#include <wrpc.h>
#include <wr-api.h>
#include <hw-specific/wrh.h>
#include <sfp.h>


enum insitu_task_state_enum {
    i_idle,
    i_acq,
    i_acq_done,
};

enum insitu_log_val_enum {
    log_off,
    log_on,
    log_crtt,
};

static enum insitu_task_state_enum insitu_task_state = i_idle;
static enum insitu_log_val_enum wrc_insitu_logging = log_off;

static void insitu_onetime_params(void);
static int insitu_pharse(const char *args[]);

static int ch_start = 0;
static int ch_end = 0;
static int ch_step = 0;
static int n_samples = 0;
static int n_samples_curr = 0;
static int ch_curr = 0;

static void insitu_help(void)
{
	pp_printf("insitu command for insitu fiber calibration\n");
	pp_printf("Available commands:\n");
	pp_printf("  on         - Turn on printout of t1-t4 timestamps.\n");
	pp_printf("  delay      - Turn on printout of Corrected Round Trip Time ((t4-t3)+(t2-t1) corrected by deltas).\n");
	pp_printf("  off        - Turn off printout of timestamps/CRTT.\n");
	pp_printf("  bts        - Print bitslide.\n");
	pp_printf("  run <start> <end> <step> <sample> - automatic sweep over number of channels. From channel <start> to (including) channel <end> with <step>. Get number of <sample>s for each channel.\n");
	pp_printf("  stop       - Stop automatic sweep over number of channels.\n");
	pp_printf("  <no param> - Toggles on/off.\n");
}

static int cmd_insitu(const char *args[])
{
	/* no arguments: invert */
	if (!args[0]) {
		wrc_insitu_logging = !wrc_insitu_logging;
		if (!wrc_insitu_logging)
			pp_printf("insitu log off\n");
		return 0;
	}

	/* arguments: bts, on, off */
	if (!strcasecmp(args[0], "bts")) {
		pp_printf("%d ps\n", ep_get_bitslide(&wrc_endpoint_dev));
	} else if (!strcasecmp(args[0], "on")) {
		wrc_insitu_logging = log_on;
		insitu_onetime_params();
	} else if (!strcasecmp(args[0], "crtt")) {
		wrc_insitu_logging = log_crtt;
		insitu_onetime_params();
	} else if (!strcasecmp(args[0], "off")) {
		wrc_insitu_logging = log_off;
		pp_printf("insitu log off\n");
	} else if (!strcasecmp(args[0], "run")) {
		return insitu_pharse(args);
	} else if (!strcasecmp(args[0], "stop")) {
		insitu_task_state = i_idle;
		/* Turn off logging */
		wrc_insitu_logging = log_off;
		pp_printf("insitu run stopped!\n");
	} else {
		insitu_help();
		return -EINVAL;
	}

	return 0;

}

static void insitu_onetime_params(void)
{
    struct pp_instance *ppi = ppg->pp_instances;
    wrh_servo_t * wrh_servo;
    wr_servo_ext_t * wr_servo_ext = NULL;
    wrh_servo = (ppi->protocol_extension == PPSI_EXT_WR
		 && ppi->extState == PP_EXSTATE_ACTIVE) ?
		    (wrh_servo_t*) ppi->ext_data : NULL;
    if (wrh_servo) {
	    wr_servo_ext = &((struct wr_data *)wrh_servo)->servo_ext;
    }
    if (wr_servo_ext) {
	pp_printf("delta txm:%lld  rxm:%lld txs:%lld rxs:%lld ",
		  pp_time_to_picos(&wr_servo_ext->delta_txm),
		  pp_time_to_picos(&wr_servo_ext->delta_rxm),
		  pp_time_to_picos(&wr_servo_ext->delta_txs),
		  pp_time_to_picos(&wr_servo_ext->delta_rxs)
		);
// 	pp_printf("setpoint:%d\n", wrh_servo->cur_setpoint_ps); /* Unable to read setpoint */
	pp_printf("\n");
    }
}

void insitu_print_t1_t2(struct pp_instance *ppi)
{
    if (ppi->state == PPS_SLAVE) {
	if (!n_samples_curr)
	    n_samples_curr++;

	if (wrc_insitu_logging == log_on) {
	    pp_printf("t1t2: %s ", time_to_string(&ppi->t1));
	    pp_printf("%s\n", time_to_string(&ppi->t2));
	}
    }
}

void insitu_print_t3_t4(struct pp_instance *ppi)
{
    if (ppi->state == PPS_SLAVE) {
	if (n_samples_curr > 0)
	    n_samples_curr++;

	if (wrc_insitu_logging == log_on) {
	    if (n_samples_curr <= 0) {
		/* Avoid printout of t3t4 as first sample */
		return;
	    }

	    pp_printf("t3t4: %s ", time_to_string(&ppi->t3));
	    pp_printf("%s\n", time_to_string(&ppi->t4));
	}

	if (wrc_insitu_logging == log_crtt) {
	    /* (t4-t3)+(t2-t1) */
	    struct pp_time pp_time_tmp = ppi->t4;
	    pp_time_sub(&pp_time_tmp, &ppi->t3);
	    pp_time_add(&pp_time_tmp, &ppi->t2);
	    pp_time_sub(&pp_time_tmp, &ppi->t1);

	    pp_printf("t3t4t2t1: %s\n", time_to_string(&pp_time_tmp));
	}

	if (n_samples_curr >= n_samples && insitu_task_state == i_acq)
	    insitu_task_state = i_acq_done;
    }
}

void insitu_task_reload(void)
{
    insitu_task_state = i_acq;
    n_samples_curr = 0;
}

int insitu_task(void)
{
    char buff_ch[4];
    uint32_t now;
    static uint32_t next_update_ticks;

    if (insitu_task_state != i_acq_done)
	return 0;

    now = timer_get_tics();
    if (time_before(now, next_update_ticks))
	return 0;

    next_update_ticks = now + TICS_PER_SECOND;

    if (ch_curr > ch_end) {
	insitu_task_state = i_idle;
	/* Turn off logging */
	wrc_insitu_logging = 0;
	pp_printf("insitu done\n");
	return 1;
    }

    /* stop PTP? */
    wrc_ptp_run(0);
    /* Set next channel */
    pp_sprintf(buff_ch, "%d", ch_curr);
    sfp_tune_ch(buff_ch);

    wrc_ptp_run(1);

    /* Restart task's state */
    insitu_task_state = i_acq;
    n_samples_curr = 0;

    /* Set correct channel */
    ch_curr += ch_step;


    return 1;
}

static int insitu_pharse(const char *args[])
{
    if (!args[1])
	goto missing_param_start;
    ch_start = atoi(args[1]);

    if (!args[2])
	goto missing_param_end;
    ch_end = atoi(args[2]);

    if (!args[3])
	goto missing_param_step;
    ch_step = atoi(args[3]);

    if (!args[4])
	goto missing_param_samples;
    n_samples = atoi(args[4]);

    pp_printf("start: %d end: %d step: %d samples %d\n", ch_start, ch_end, ch_step, n_samples);

    /* We probably want to get samples */
    wrc_insitu_logging = 1;

    insitu_task_state = i_acq_done;
    n_samples_curr = 0;
    ch_curr = ch_start;

    return 0;

missing_param_start:
    pp_printf("param missing:channel start\n");
missing_param_end:
    pp_printf("param missing:channel end\n");
missing_param_step:
    pp_printf("param missing:channel step\n");
missing_param_samples:
    pp_printf("param missing:Numer of samples\n");

    return -EINVAL;
}

DEFINE_WRC_COMMAND(insitu) = {
	.name = "insitu",
	.exec = cmd_insitu,
};
