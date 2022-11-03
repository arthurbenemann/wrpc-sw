/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <errno.h>
#include <string.h>
#include <wrpc.h>
#include "wrc_ptp.h"
#include "shell.h"

#ifdef CONFIG_CMD_PTP_ADV
#define HAS_CMD_PTP_ADV 1
#else
#define HAS_CMD_PTP_ADV 0
#endif

#ifdef CONFIG_PTP_OPT_OVERWRITE_ATTRIBUTES
#define HAS_PTP_OPT_OVERWRITE_ATTRIBUTES 1
#else
#define HAS_PTP_OPT_OVERWRITE_ATTRIBUTES 0
#endif

#define USE_CMD_PARAM -1

static const struct subcmd {
	const char *name;
	int (*fun)(int);
	int arg;
} subcmd[] = {
	{"start", wrc_ptp_run, 1},
	{"stop", wrc_ptp_run, 0},
	{"e2e", wrc_ptp_sync_mech, MECH_E2E},
	{"delay", wrc_ptp_sync_mech, MECH_E2E},
#ifdef CONFIG_P2P
	{"p2p", wrc_ptp_sync_mech, MECH_P2P},
	{"pdelay", wrc_ptp_sync_mech, MECH_P2P},
#endif
	{"gm", wrc_ptp_set_mode, WRC_MODE_GM},
	{"master", wrc_ptp_set_mode, WRC_MODE_MASTER},
	{"slave", wrc_ptp_set_mode, WRC_MODE_SLAVE},
#ifdef CONFIG_CMD_PTP_ADV
	 /* use next param as a val for func */
	{"prio1",    wrc_ptp_set_prio1, USE_CMD_PARAM},
	{"prio2",    wrc_ptp_set_prio2, USE_CMD_PARAM},
	{"domain",   wrc_ptp_set_domain_number, USE_CMD_PARAM},
#ifdef CONFIG_PTP_OPT_OVERWRITE_ATTRIBUTES
	{"class",    wrc_ptp_set_clock_class, USE_CMD_PARAM},
	{"accuracy", wrc_ptp_set_clock_accuracy, USE_CMD_PARAM},
	{"allan",    wrc_ptp_set_clock_allan_variance, USE_CMD_PARAM},
	{"tsource",  wrc_ptp_set_time_source, USE_CMD_PARAM},
#endif /* CONFIG_PTP_OPT_OVERWRITE_ATTRIBUTES */
#endif /* CONFIG_CMD_PTP_ADV */
#ifdef CONFIG_ABSCAL
	{"abscal", wrc_ptp_set_mode, WRC_MODE_ABSCAL},
#endif
};

static const char * const is_run[] = {"stopped", "running"};
static const char * const is_mech[] = {[MECH_E2E] = "e2e", [MECH_P2P] = "p2p"};
static const char * const is_mode[] = {[WRC_MODE_GM] = "gm",
				       [WRC_MODE_MASTER] = "master",
				       [WRC_MODE_SLAVE] = "slave"
#ifdef CONFIG_ABSCAL
				       , [WRC_MODE_ABSCAL] = "abscal"
#endif
			 };

static int cmd_ptp(const char *args[])
{
	int i, j, ret;
	const struct subcmd *c;
	int l_arg;


	if (!args[0]) {
		pp_printf("%s; %s %s\n",
			  is_run[wrc_ptp_run(-1)],
			  is_mech[wrc_ptp_sync_mech(-1)],
			  is_mode[wrc_ptp_get_mode()]);
		if (HAS_CMD_PTP_ADV) {
			wrc_ptp_set_prio1(USE_CMD_PARAM);
			wrc_ptp_set_prio2(USE_CMD_PARAM);
			wrc_ptp_set_domain_number(USE_CMD_PARAM);
			if (HAS_PTP_OPT_OVERWRITE_ATTRIBUTES) {
				wrc_ptp_set_clock_class(USE_CMD_PARAM);
				wrc_ptp_set_clock_accuracy(USE_CMD_PARAM);
				wrc_ptp_set_clock_allan_variance(USE_CMD_PARAM);
				wrc_ptp_set_time_source(USE_CMD_PARAM);
			}
		}
		return 0;
	}

	for (j = 0; args[j]; j++) {
		for (i = 0, c = subcmd; i < ARRAY_SIZE(subcmd); i++, c++) {
			if (!strcasecmp(args[j], c->name)) {
				l_arg = c->arg;
				/* if c->arg is USE_CMD_PARAM use next arg
				 * as a parameter */
				if (l_arg == USE_CMD_PARAM && args[++j])
					l_arg = atoi(args[j]);
				ret = c->fun(l_arg);
				if (ret < 0)
					return ret;
				break;
			}
		}
		if (i == ARRAY_SIZE(subcmd)) {
			pp_printf("Unknown subcommand \"%s\"\n", args[j]);
			return -EINVAL;
		}
	}
	return 0;
}

DEFINE_WRC_COMMAND(ptp) = {
	.name = "ptp",
	.exec = cmd_ptp,
};
DEFINE_WRC_COMMAND(mode) = {
	.name = "mode",
	.exec = cmd_ptp,
};
