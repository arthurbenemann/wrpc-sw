/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2021 CERN
 * Author: Adam Wujek
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <inttypes.h>
#include <wrc.h>
#include <dev/w1.h>
#include <ppsi/ppsi.h>
#include <wrpc.h>
#include <wr-api.h>
#include <dev/minic.h>
#include <softpll_ng.h>
#include <dev/syscon.h>
#include <dev/pps_gen.h>
#include <dev/onewire.h>
#include <dev/endpoint.h>
#include <dev/netif.h>
#include <temperature.h>
#include "wrc_ptp.h"
#include "hal_exports.h"
#include "lib/ipv4.h"
#include "shell.h"
#include "revision.h"
#include "wrc_global.h"

#ifndef CONFIG_PRINTF_FULL
#error ("WRPC monitor requires full version of pp_printf implementation")
#endif


#define WRC_MONITOR_REFRESH_PERIOD (1 * TICS_PER_SECOND)
#define WRC_DIAG_REFRESH_PERIOD (1 * TICS_PER_SECOND)

#define DESCRIPTION_MAIN 1
#define DESCRIPTION_SERVO 2
#define DESCRIPTION_WR_SERVO 4

/* protocol extension data */
#define IS_PROTO_EXT_INFO_AVAILABLE(proto_id) \
	((proto_id < sizeof(proto_ext_info) / sizeof(struct proto_ext_info_t)) \
			&& (proto_ext_info[proto_id].valid == 1))


/* internal "last", exported to shell command */
uint32_t wrc_stats_last;
extern struct pp_instance ppi_static;
extern struct pp_globals ppg_static;
extern struct pp_globals *ppg;
extern char *wrc_hw_name;
static int prev_gui_description = 0;
static int gui_description = 1;
static uint32_t next_update_ticks;
/* refresh period for _gui_ and _stat_ commands */
int wrc_ui_refperiod = WRC_MONITOR_REFRESH_PERIOD;

void print_main_description(void);
void print_main_data(void);
void print_servo_description(void);
void print_servo_data(struct pp_instance *ppi);
void print_aux_data(void);
int wrc_wr_diags(void);


struct proto_ext_info_t {
	int valid;
	char *ext_name; /* Extension name */
	char short_ext_name; /* Very short extension name - just one character */
	int servo_ext_size; /* Size of the extension */
	int ipc_cmd_tacking; /* Command to enable/disable servo tacking*/
	int track_onoff;     /* Tracking on/off */
	time_t lastt;
	int last_count;
};

static struct proto_ext_info_t proto_ext_info [] = {
		[PPSI_EXT_NONE] = {
				.valid = 1,
				.ext_name = "PTP",
				.short_ext_name = 'P',
				.ipc_cmd_tacking = -1, /* Invalid */
		},

#if CONFIG_HAS_EXT_WR
		[PPSI_EXT_WR] = {
				.valid = 1,
				.ext_name ="White-Rabbit",
				.short_ext_name ='W',
				.servo_ext_size = sizeof(struct wr_data),
// 				.ipc_cmd_tacking = PTPDEXP_COMMAND_WR_TRACKING,
				.track_onoff = 1,
		},
#endif
#if CONFIG_HAS_EXT_L1SYNC
		[PPSI_EXT_L1S] = {
				.valid = 1,
				.ext_name ="L1Sync",
				.short_ext_name ='L',
				.servo_ext_size = sizeof(struct l1e_data),
// 				.ipc_cmd_tacking = PTPDEXP_COMMAND_L1SYNC_TRACKING,
				.track_onoff = 1,
		},
#endif

};


/* define size of pp_instance_state_to_name as a last element + 1 */
#define PP_INSTANCE_STATE_MAX (sizeof(pp_instance_state_to_name) / sizeof(char *))

/* define conversion array for the field state in the struct pp_instance */
static char *pp_instance_state_to_name[] = {
	/* from ppsi/include/ppsi/ieee1588_types.h, enum pp_std_states */
	/* PPS_END_OF_TABLE = 0 */
	[PPS_END_OF_TABLE] =      "EOT       ",
	[PPS_INITIALIZING] =      "INITING   ",
	[PPS_FAULTY] =            "FAULTY    ",
	[PPS_DISABLED] =          "DISABLED  ",
	[PPS_LISTENING] =         "LISTENING ",
	[PPS_PRE_MASTER] =        "PRE_MASTER",
	[PPS_MASTER] =            "MASTER    ",
	[PPS_PASSIVE] =           "PASSIVE   ",
	[PPS_UNCALIBRATED] =      "UNCALIBRAT",
	[PPS_SLAVE] =             "SLAVE     ",
	NULL
	};

#define EMPTY_EXTENSION_STATE_NAME "          "

#if CONFIG_HAS_EXT_L1SYNC
static char * l1e_instance_extension_state[]={
		[__L1SYNC_MISSING   ] = "INVALID   ",
		[L1SYNC_DISABLED    ] = "DISABLED  ",
		[L1SYNC_IDLE        ] = "IDLE      ",
		[L1SYNC_LINK_ALIVE  ] = "LINK ALIVE",
		[L1SYNC_CONFIG_MATCH] = "CFG MATCH ",
		[L1SYNC_UP          ] = "UP        ",
		NULL
};
#define L1S_INSTANCE_EXTENSION_STATE_MAX (sizeof (l1e_instance_extension_state)/sizeof(char *))

#endif

static char * timing_mode_state[] = {
		[WRH_TM_GRAND_MASTER]=     "GM",
		[WRH_TM_FREE_MASTER]=      "FR",
		[WRH_TM_BOUNDARY_CLOCK]=   "BC",
		[WRH_TM_DISABLED]=         "--",
		NULL
	};

#if CONFIG_HAS_EXT_WR
static char * wr_instance_extension_state[]={
		[WRS_IDLE ]=              "IDLE      ",
		[WRS_PRESENT] =           "WR_PRESENT",
		[WRS_S_LOCK] =            "WR_S_LOCK ",
		[WRS_M_LOCK] =            "WR_M_LOCK ",
		[WRS_LOCKED] =            "WR_LOCKED ",
		[WRS_CALIBRATION] =       "WR_CAL-ION",
		[WRS_CALIBRATED] =        "WR_CAL-ED ",
		[WRS_RESP_CALIB_REQ] =    "WR_RSP_CAL",
		[WRS_WR_LINK_ON] =        "WR_LINK_ON",
		NULL
};
#define WR_INSTANCE_EXTENSION_STATE_MAX (sizeof (wr_instance_extension_state) / sizeof(char *))

#endif

static char *prot_detection_state_name[]={
		"NONE   ", /* No meaning. No extension present */
		"WA_MSG ", /* Waiting first message */
		"PD_IPRG", /* Protocol detection  */
		"EXT_ON ", /* Protocol detected */
		"EXT_OFF" /* Protocol not detected */
};

static struct desired_state_t{
	char *str_state;
	int state;
} 

desired_states[] = {
	{ "initializing", PPS_INITIALIZING},
	{ "faulty",       PPS_FAULTY},
	{ "disabled",     PPS_DISABLED},
	{ "listening",    PPS_LISTENING},
	{ "pre-master",   PPS_PRE_MASTER},
	{ "master",       PPS_MASTER},
	{ "passive",      PPS_PASSIVE},
	{ "uncalibrated", PPS_UNCALIBRATED},
	{ "slave",        PPS_SLAVE},
	{}
};

static inline char * timeToString_ps_as_ns(struct pp_time *time, char *buf)
{
	if (!is_incorrect(time)) {
		return time_to_string(time);
	} else {
		sprintf(buf, "--Incorrect--");
	}
	return buf;
}

static inline int extensionStateColor(struct pp_instance *ppi)
{
	if (ppi->protocol_extension == PPSI_EXT_NONE) {
		return C_GREEN; /* No extension */
	}
	switch (ppi->extState) {
	case PP_EXSTATE_ACTIVE :
		return C_GREEN;
	case PP_EXSTATE_PTP :
		return C_WHITE;
	case PP_EXSTATE_DISABLE :
	default:
		return C_RED;
	}
}

static char *getStateAsString(char *p[], int index)
{
	int i, len;
	char *errMsg = "?????????????????????";

	len = strlen(p[0]);
	for (i = 0; ; i++) {
		if (p[i] == NULL)
			return errMsg + strlen(errMsg) - len;
		if (i == index)
			return p[index];
	}
}

static char *optimized_pp_time_toString_ps_as_ns(struct pp_time *pptime, char *buf)
{
	char lbuf[128];

	if (pptime->secs)
		sprintf(buf,"%s sec ", timeToString_ps_as_ns(pptime, lbuf));
	else
		sprintf(buf,"%s nsec", interval_to_string(pp_time_to_interval(pptime)));
	return buf;
}

char * convert_ps_to_str_ns(char *buff, int64_t num)
{
	int i;
	int len;
	char *p;
	sprintf(buff, "% 05Ld", num);
	len = strlen(buff);
	p = buff + len;
	for (i = 0; i < 4; i++, p--) {/* move EOL and 3 chars */
		*(p + 1) = *(p);
	}
	*(p + 1) = '.';
	return buff;
}

int time(void *a)
{
    return 1;
}

void redraw_gui(void)
{
	prev_gui_description = 0;
	next_update_ticks = 0;
}

int wrc_mon_gui(void)
{
	static uint32_t last_servo_count;
	uint32_t now;
	struct pp_servo *s = SRV(ppg->pp_instances);

	/* update on timeout or servo update */
	now = timer_get_tics();

	/* print new values only if time elapsed or servo's update_count
	 * increased */
	if (prev_gui_description != gui_description) {
		term_clear();
		if (gui_description & DESCRIPTION_MAIN) {
			print_main_description();
		}
		if (gui_description & DESCRIPTION_SERVO) {
			print_servo_description();
		}
		next_update_ticks = now;
		term_clear_to_end();
		prev_gui_description = gui_description;
		return 1;
	}

	if (time_before(now, next_update_ticks)
	    && last_servo_count == s->update_count)
		return 0;

	next_update_ticks = now + wrc_ui_refperiod;
	last_servo_count = s->update_count;
	prev_gui_description = gui_description;
	gui_description = DESCRIPTION_MAIN;
	print_main_data();

	/* Print servo */
	/* FIXME: add support of multiple instances */
	print_servo_data(ppg->pp_instances);
	print_aux_data();
	/* clear colors */
	pp_printf("\e[m\n");
	/* clear till the end of a screen */
	term_clear_to_end();

	return 1;
}

void print_main_description(void)
{
	int i;
	int ndevs;

	pcprintf(1, 1, C_BLUE, "%s WR PTP Core Sync Monitor %s",
		wrc_hw_name, build_revision);
	cprintf(C_MAGENTA, "\nEsc or q = exit; r = redraw GUI");

	cprintf(C_BLUE, "\n\nTAI Time:%22sUTC offset:", "");

	pp_printf("\nPLL mode:%22sPLL state:\n", "");

	ndevs = netif_get_device_count();

	/*show_ports */
	cprintf(C_CYAN, "------+-------------------+-------------------------+---------+---------+-----\n");
	pp_printf(      "Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN\n");
	pp_printf(      "------+-------------------+-------------------------+---------+---------+-----\n");

	for (i = 0 ; i < ndevs; i++) {
		/* reuse the string above, strings between "|" will be overwritten anyway */
		pp_printf("Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN\n");
	}

	pp_printf("\n----- HAL ---|---------------- PPSI -------------------------------------------------\n");
	pp_printf(  " Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro \n");
	pp_printf(  "------+------+--------------+-------------------+------------------------------+-----\n");
	for (i = 0 ; i < ndevs; i++) {
		/* reuse the string above, strings between "|" will be overwritten anyway */
		pp_printf(" Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro \n");
	}

	cprintf(C_BLUE, "Pro - Protocol mapping: V-Ethernet over "
			"VLAN; U-UDP; R-Ethernet\n");
	
	cprintf(C_CYAN, "\n--------------------------- Synchronization status ----------------------------");
}

void print_main_data(void)
{
	struct wrc_port_state state;
	int tx, rx;
	int leap_sec, tmp;
	uint64_t sec;
	uint32_t nsec;
	char buf[20];
	uint8_t mac[ETH_ALEN];
	int ndevs;
	int i;

	const char *pll_locking_state_name;
	shw_pps_gen_get_time(&sec, &nsec);

	/* TAI Time */
	pcprintf(4, 11, C_WHITE, "%s", format_time(sec, TIME_FORMAT_SORTED));

	
	/* UTC offset */
	wrc_ptp_get_leapsec(&leap_sec , &tmp /* dummy */);
	pprintf(4, 44, "%d", leap_sec);

	/* Timing mode  */
	pprintf(5, 11, getStateAsString(timing_mode_state, WRPC_ARCH_G(ppg)->timingMode));

	/* PLL locking state */
	if (spll_check_lock(0))
		pll_locking_state_name = "Locked ";
	else {
		pll_locking_state_name = "Locking";
	}
	pprintf(5, 44, "%s", pll_locking_state_name);

	ndevs = netif_get_device_count();

	/* show_ports
	------+-------------------+-------------------------+---------+---------+-----
	Iface |        MAC        |       IP (source)       |    RX   |    TX   | VLAN
	------+-------------------+-------------------------+---------+---------+-----
	*/

	for (i = 0 ; i < ndevs; i++) {
		struct wrc_netif_device *ndev = netif_get_device(i);
		int port_up = ndev->link_state == NETIF_LINK_UP;

		if (port_up) {
			pcprintf(9, 1, C_GREEN, " %s: ", ndev->name);
		} else {
			pcprintf(9, 1, C_RED, "*%s: ", ndev->name);
		}

		if (i == 0) /* FIXME: should be independent for each interface */
		{
			ep_get_mac_addr(&wrc_endpoint_dev, mac);
			format_mac(buf, mac);
			pcprintf(9, 9, C_MAGENTA, "%s", buf);
			if (HAS_IP && port_up) {
				uint8_t ip[INET_ALEN];

				getIP(ip);
				format_ip(buf, ip);
				switch (*ip_status) {
				case IP_TRAINING:
					pcprintf(9, 29, C_RED,   "BOOTP running          ");
					break;
				case IP_OK_BOOTP:
					pcprintf(9, 29, C_GREEN, "%16s(BOOTP)", buf);
					break;
				case IP_OK_STATIC:
					pcprintf(9, 29, C_GREEN, "%15s(static)", buf);
					break;
				}
			} else
				pcprintf(9, 29, C_GREEN, "                       ");

			minic_get_stats(&tx, &rx);
			pcprintf(9, 55, C_MAGENTA, "%7d", rx);
			pprintf(9, 65, "%7d", tx);
			pprintf(9, 75, "%4d", *wrc_vlan_number);
		}

	}
	/* 
	----- HAL ---|---------------- PPSI -------------------------------------------------
	 Iface| Freq |    Config    | MAC of peer port  |    PTP/EXT/PDETECT States    | Pro 
	------+------+--------------+-------------------+------------------------------+----- */

	for (i = 0 ; i < ndevs; i++) {
		struct wrc_netif_device *ndev = netif_get_device(i);
		int port_up = ndev->link_state == NETIF_LINK_UP;
		int color;

		if (port_up) {
			pcprintf(14, 1, C_GREEN, " %s: ", ndev->name);
		} else {
			pcprintf(14, 1, C_RED,   "*%s: ", ndev->name);
		}

		/* FIXME: should be independent for each interface */
		wrpc_get_port_state(&state, NULL);

		if (state.locked)
			pcprintf(14, 9, C_GREEN, "Lock");
		else
			pcprintf(14, 9, C_RED,   "    ");

		
/* ----------------------------------------------------------------------------------------------------------------------- */
		/*
		 * Actually, what is interesting is the PTP state.
		 * For this lookup, the port in ppsi shmem
		 */
		/* Assume one instance per port */
		/* FIXME: add support of more ports */
//		for (j = 0; j < ppg->nlinks; j++) {
			{
			char str_config[15];
			/* so far support only for one instance */
			struct pp_instance *ppi_pt = ppg->pp_instances;
			int proto_extension = ppi_pt->protocol_extension;
			struct proto_ext_info_t *pe_info = IS_PROTO_EXT_INFO_AVAILABLE(proto_extension) ? &proto_ext_info[proto_extension] :  &proto_ext_info[0] ;
			unsigned char *p = ppi_pt->activePeer;
			char * extension_state_name = EMPTY_EXTENSION_STATE_NAME;
			char proto;
			char mac_buf[20];

#if 0 /* FIXME: only one instance so far */
			if (strcmp(if_name,
					ppi->cfg.iface_name)) {
				/* Instance not for this interface
				    * skip */
				continue;
			}
#endif
			// Evaluate the instance configuration
			strcpy(str_config,"unknown");
			if (is_slaveOnly(ppg->defaultDS)) {
				strncpy(str_config, "slaveOnly", sizeof(str_config) - 1);
			} else {
				if (is_externalPortConfigurationEnabled(ppg->defaultDS)) {
					int s = 0;
					for (s = 0; s < sizeof(desired_states) / sizeof(struct desired_state_t); s++) {
						if (desired_states[s].state == ppi_pt->externalPortConfigurationPortDS.desiredState) {
							strncpy(str_config, desired_states[s].str_state, sizeof(str_config) - 1);
							break;
						}
					}

				} else {
					if (is_masterOnly(ppi_pt->portDS)) {
						strncpy(str_config, "masterOnly", sizeof(str_config) - 1);
					} else {
						strncpy(str_config, "auto", sizeof(str_config) - 1);
					}
				}
			}
			str_config[sizeof(str_config) - 1] = 0; // Force the string to be well terminated
			pcprintf(14, 16, C_WHITE, "%-12s", str_config);

			/* peer not implemented */
			pprintf(14, 31, format_mac(mac_buf, p));

			pcprintf(14, 51, C_GREEN, "%s/", getStateAsString(pp_instance_state_to_name, ppi_pt->state));
			/* print extension state */
			switch (ppi_pt->protocol_extension) {
#if CONFIG_HAS_EXT_WR
			case PPSI_EXT_WR :
			{
				portDS_t *portDS = ppi_pt->portDS;
				struct wr_dsport *extPortDS;

				extension_state_name = getStateAsString(wr_instance_extension_state, - 1); // Default value

				if (portDS) {
					if ((extPortDS = portDS->ext_dsport))
						extension_state_name = getStateAsString(wr_instance_extension_state, extPortDS->state);
				}
				break;
			}
#endif
#if CONFIG_HAS_EXT_L1SYNC
			case PPSI_EXT_L1S :
			{
				portDS_t *portDS;

				extension_state_name = getStateAsString(l1e_instance_extension_state, - 1); // Default value
				if ((portDS = wrs_shm_follow(ppsi_head, ppi->portDS))) {
					l1e_ext_portDS_t *extPortDS;

					if ((extPortDS = wrs_shm_follow(ppsi_head, portDS->ext_dsport))) {
							extension_state_name = getStateAsString(l1e_instance_extension_state, extPortDS->basic.L1SyncState);
					}
				}
				break;
			}
#endif
			}
			pp_printf("%s/%s", extension_state_name, getStateAsString(prot_detection_state_name, ppi_pt->pdstate));

			/* proto */
			switch (ppi_pt->proto) {
			case PPSI_PROTO_RAW:
				proto = 'R';
				break;
			case PPSI_PROTO_UDP:
				proto = 'U';
				break;
			case PPSI_PROTO_VLAN:
				proto = 'V';
				break;
			default:
				proto = '?';
			}

			pcprintf(14, 82, C_WHITE, "%c", proto);
			color = extensionStateColor(ppi_pt);
			cprintf(color, "-%c", pe_info->short_ext_name);
		}
/* ----------------------------------------------------------------------------------------------------------------------- */
	}

	return;
}

void print_aux_data(void)
{
	int n_out, i;
	struct spll_aux_clock_status aux_stat;

	spll_get_num_channels(NULL, &n_out);

	for (i = 0; i < n_out - 1; i++) {
		cprintf(C_MAGENTA, "\n\nAux clock %d status:        ", i);

		aux_stat = spll_get_aux_status(i);

		if (aux_stat.flags & SPLL_AUX_SLAVE_ENABLED)
			cprintf(C_GREEN, "enabled");

		if (aux_stat.flags & SPLL_AUX_TRACKING_ENABLED)
			cprintf(C_GREEN, "tracking source");

		if (aux_stat.flags & SPLL_AUX_SLAVE_LOCKED)
			cprintf(C_GREEN, ", locked");

		if (aux_stat.flags & SPLL_AUX_TRACKING_READY)
		{
			cprintf(C_GREEN, ", ready");
			cprintf(C_WHITE, " (AUX-to-WR offset: %d ps)", aux_stat.phase);
		}
	}

	return;
}

void print_servo_description()
{
	pcprintf(18, 1, C_BLUE, "Servo state:\n");

	cprintf(C_CYAN, "\n--- Timing parameters ---------------------------------------------------------\n");

	cprintf(C_BLUE, "meanDelay        :\n");

	pp_printf("delayMS          :\n");
	pp_printf("delayMM          :\n");

	//pp_printf("Estimated link length:     ");

	pp_printf("delayAsymmetry   :\n");
	pp_printf("delayCoefficient :");
	pprintf(25, 45, "fpa\n");

	pp_printf("ingressLatency   :\n");
	pp_printf("egressLatency    :\n");
	pp_printf("semistaticLatency:\n");
	pp_printf("offsetFromMaster :\n");
	if (gui_description & DESCRIPTION_WR_SERVO) {
		pp_printf("Phase setpoint   :\n");
		pp_printf("Skew             :\n");
	}
	pp_printf("Update counter   :\n");
	if (gui_description & DESCRIPTION_WR_SERVO) {
		pp_printf("Master PHY delays TX:\n"); /* RX: */
		pp_printf("Slave  PHY delays TX:\n"); /* RX: */
	}
}

void print_servo_data(struct pp_instance *ppi)
{
	wrh_servo_t * wrh_servo;
	wr_servo_ext_t * wr_servo_ext = NULL;
	char buf[128];
	int row_offset;
	int proto_extension = ppi->extState!= PP_EXSTATE_DISABLE ? ppi->protocol_extension : PPSI_EXT_NONE;
	struct proto_ext_info_t *pe_info = IS_PROTO_EXT_INFO_AVAILABLE(proto_extension) ? &proto_ext_info[proto_extension] :  &proto_ext_info[0];

	/* --------------------------- Synchronization status ---------------------------- */
	pprintf(18, 1, "");
	if (ppi->state != PPS_SLAVE || !(ppi->servo->flags & PP_SERVO_FLAG_VALID)) {
		cprintf(C_RED, "Link down, master mode or sync info not valid");
		return;
	}

	wrh_servo = (ppi->protocol_extension == PPSI_EXT_WR && ppi->extState == PP_EXSTATE_ACTIVE) ?
			(wrh_servo_t*) ppi->ext_data : NULL;

	/* should print servio description */
	gui_description |= DESCRIPTION_SERVO;


	if (wrh_servo) {
		wr_servo_ext = &((struct wr_data *)wrh_servo)->servo_ext;
	}

	/* should print WR servio description */
	gui_description |= wrh_servo ? DESCRIPTION_WR_SERVO : 0;
	
	/* Avoid printing new data if change in description is expected.
	 * This avoids extra redraw of data values */
	if(prev_gui_description
		!= (gui_description 
		    & (DESCRIPTION_MAIN
		       | DESCRIPTION_SERVO
		       | DESCRIPTION_WR_SERVO)
		   )
	  )
		return;

	if (pe_info->lastt && time(NULL) - pe_info->lastt > 5) {
		pcprintf(18, 23, C_RED, "--- not updating ---\n");
	} else {
		pcprintf(18, 23, C_WHITE, "%s:%s: %s%-15s\n",
				ppi->cfg.iface_name,
				    pe_info->ext_name,
				    ppi->servo->servo_state_name,
				    ppi->servo->flags & PP_SERVO_FLAG_WAIT_HW ?
				" (wait for hw)" : "");
	}

	/* "tracking disabled" is just a testing tool */
	if (wrh_servo && !wrh_servo->tracking_enabled)
		cprintf(C_RED, "Tracking forcibly disabled\n");
	else
		pp_printf("\e[K"); /* clear till the end of a line */
		

	/* +- Timing parameters --------------------------------------------------------- */

	pcprintf(21, 20, C_WHITE, "%19s nsec", interval_to_string(ppg->currentDS->meanDelay));

	/*delayMS */
	pcprintf(22, 20, C_WHITE,"%24s", optimized_pp_time_toString_ps_as_ns(&ppi->servo->delayMS, buf));	
	{
		struct pp_time *delayMM = wr_servo_ext ?
				&wr_servo_ext->rawDelayMM :
				&ppi->servo->delayMM;
		/* delayMM */
		pcprintf(23, 20, C_WHITE,"%24s", optimized_pp_time_toString_ps_as_ns(delayMM, buf));
	}

	//cprintf(C_BLUE, "Estimated link length:     ");
	/* (RTT - deltas) / 2 * c / ri
	    c = 299792458 - speed of light in m/s
	    ri = 1.4682 - refractive index for fiber g.652. However,
			experimental measurements using long (~5km) and
			short (few m) fibers gave a value 1.4827
	    */
	//cprintf(C_WHITE, "%10.2f meters\n",
	//	crtt / 2 / 1e6 * 299.792458 / 1.4827);


	/* delayAsymmetry */
	pcprintf(24, 20, C_WHITE, "%19s nsec",   interval_to_string(ppi->portDS->delayAsymmetry));
	/* delayCoefficient */
	pcprintf(25, 23, C_WHITE, "%s", relative_interval_to_string(ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient));
	/* fpa */
	pcprintf(25, 51, C_WHITE, "%Lu", ppi->asymmetryCorrectionPortDS.scaledDelayCoefficient); /* print as unsigned! */

	/* ingressLatency */
	pcprintf(26, 20, C_WHITE, "%19s nsec",   interval_to_string(ppi->timestampCorrectionPortDS.ingressLatency));
	/* egressLatency */
	pcprintf(27, 20, C_WHITE, "%19s nsec",   interval_to_string(ppi->timestampCorrectionPortDS.egressLatency));
	/* semistaticLatency */
	pcprintf(28, 20, C_WHITE, "%19s nsec",   interval_to_string(ppi->timestampCorrectionPortDS.semistaticLatency));

	/*if (0) {
		cprintf(C_BLUE, "Fiber asymmetry:   ");
		cprintf(C_WHITE, "%.3f nsec\n",
			ss.fiber_asymmetry/1000.0);
	}*/

	/* offsetFromMaster */
	pcprintf(29, 20, C_WHITE, "%19s nsec", interval_to_string (ppg->currentDS->offsetFromMaster));
	row_offset = 30;
	if (wrh_servo) {
		/* Phase setpoint */
		pcprintf(30, 20, C_WHITE, "%19s nsec", convert_ps_to_str_ns(buf, (int64_t) wrh_servo->cur_setpoint_ps));


		/* Skew */
		pcprintf(31, 20, C_WHITE, "%19s nsec", convert_ps_to_str_ns(buf, wrh_servo->skew_ps));
		row_offset += 2;
	}

	 /* Update counter */
	pcprintf(row_offset, 23, C_WHITE, "%16u times", ppi->servo->update_count);
	if (ppi->servo->update_count != pe_info->last_count) {
		pe_info->lastt = time(NULL);
		pe_info->last_count = ppi->servo->update_count;
	}

	if (wrh_servo) {
		/* Master PHY delays TX */
		pcprintf(33, 26, C_WHITE,"%22s", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_txm, buf));
		cprintf(C_BLUE, "  RX:");
		/* print and clear till the end of a line */
		cprintf(C_WHITE,"%22s\e[K", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_rxm, buf));

		/* Slave  PHY delays TX */
		pcprintf(34, 26, C_WHITE,"%22s", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_txs, buf));
		cprintf(C_BLUE, "  RX:");
		/* print and clear till the end of a line */
		cprintf(C_WHITE,"%22s\e[K", optimized_pp_time_toString_ps_as_ns(&wr_servo_ext->delta_rxs, buf));
	}

}

int wrc_log_stats(void)
{
	struct wrc_port_state state;
	int tx, rx;
	struct spll_aux_clock_status aux_stat;
	uint64_t sec;
	uint32_t nsec;
	static uint32_t last_update_tick;
	int n_out;
	int i;
	struct pp_servo *s = SRV(ppg->pp_instances);
	wrh_servo_t * wrh_servo;
	wr_servo_ext_t * wr_servo_ext = NULL;


	/* stats update condition for Slave mode */
	if (wrc_stats_last == s->update_count && ptp_mode == WRC_MODE_SLAVE) {
		last_update_tick = 0;
		return 0;
	}

	if (!wrc_stat_running) {
		last_update_tick = 0;
		return 0;
	}

	/* stats update condition for Master mode */
	if (wrc_task_not_yet(&last_update_tick, wrc_ui_refperiod))
		return 0;


	/* Print only one time */
	if (wrc_stat_running == -1)
		wrc_stat_running = 0;

	wrc_stats_last = s->update_count;

	shw_pps_gen_get_time(&sec, &nsec);
	wrpc_get_port_state(&state, NULL);
	minic_get_stats(&tx, &rx);

	pp_printf("lnk:%d rx:%d tx:%d ", (wrc_global_link.link_up == NETIF_LINK_UP), rx, tx);
	pp_printf("lock:%d ", state.locked ? 1 : 0);
	pp_printf("ptp:%s ", get_state_as_string(&ppi_static, ppi_static.state));

	if (ptp_mode == WRC_MODE_SLAVE) {
		pp_printf("sv:%d ", (s->flags & PP_SERVO_FLAG_VALID) ? 1 : 0);
		pp_printf("ss:'%s' ", s->servo_state_name);
	}

	spll_get_num_channels(NULL, &n_out);

	for (i = 0; i < n_out - 1; i++) {
		aux_stat = spll_get_aux_status(i);
		pp_printf("aux%d:%08x%08x ", i, (int) aux_stat.flags, (int) aux_stat.phase);
	}

	/* fixme: clock is not always 125 MHz */
	pp_printf("sec:%d nsec:%09d ", (int) sec, (int) nsec);
	wrh_servo = (ppi_static.protocol_extension == PPSI_EXT_WR && ppi_static.extState == PP_EXSTATE_ACTIVE) ?
			(wrh_servo_t*) ppi_static.ext_data : NULL;

	if (wrh_servo) {
		wr_servo_ext = &((struct wr_data *)wrh_servo)->servo_ext;
	}

	if (ptp_mode == WRC_MODE_SLAVE) {
		struct pp_time crtt;

		/* RTT */
		pp_printf("mu:%Ld ", pp_time_to_picos(&wr_servo_ext->rawDelayMM));

		pp_printf("dms:%Ld ", pp_time_to_picos(&s->delayMS));

		pp_printf("dtxm:%d drxm:%d ",
			  (int) pp_time_to_picos(&wr_servo_ext->delta_txm),
			  (int) pp_time_to_picos(&wr_servo_ext->delta_rxm));
		pp_printf("dtxs:%d drxs:%d ",
			  (int) pp_time_to_picos(&wr_servo_ext->delta_txs),
			  (int) pp_time_to_picos(&wr_servo_ext->delta_rxs));
		pp_printf("asym:%Ld ", interval_to_picos(ppi_static.portDS->delayAsymmetry));

		crtt = wr_servo_ext->rawDelayMM;
		pp_time_sub(&crtt, &wr_servo_ext->delta_txm);
		pp_time_sub(&crtt, &wr_servo_ext->delta_rxm);
		pp_time_sub(&crtt, &wr_servo_ext->delta_txs);
		pp_time_sub(&crtt, &wr_servo_ext->delta_rxs);

		/* Cable RTT */
		pp_printf("crtt:%Ld ", pp_time_to_picos(&crtt));
		/* Clock offset */
		pp_printf("cko:%d ", (int) pp_time_to_picos(&s->offsetFromMaster));
		pp_printf("setp:%d ", (int) wrh_servo->cur_setpoint_ps);
		pp_printf("ucnt:%d ", (int) s->update_count);
		pp_printf("bslide:%d ", ep_get_bitslide(&wrc_endpoint_dev));
	}

	pp_printf("hd:%d md:%d ad:%d ", spll_get_dac(-1), spll_get_dac(0),
		spll_get_dac(1));

	if (HAS_TEMP_SENSORS) {
		int32_t temp;

		temp = wrc_temp_get("pcb");
		pp_printf("temp:%d.%04d C", (int) (temp >> 16),
			  (int) ((temp & 0xffff) * 10 * 1000 >> 16));
	}

	pp_printf("\n");

	return 1;
}


int wrc_wr_diags(void)
{
	struct wrc_port_state ps;
	static uint32_t last_update_tick;
	int tx, rx;
	uint64_t sec;
	uint32_t nsec;
	int n_out;
	uint32_t aux_stat = 0;
	int temp = 0, valid = 0, snapshot = 0, i;

	struct pp_instance *ppi = ppg->pp_instances;
	valid    = wdiag_get_valid();
	snapshot = wdiag_get_snapshot();

	/* if the data is snapshot and there is already valid data, do not
	 * refresh */
	if (valid & snapshot)
	      return 0;

	/* ***************** lock data from reading by user **************** */
	/* stats update condition */
	if (wrc_task_not_yet(&last_update_tick, WRC_DIAG_REFRESH_PERIOD))
		return 0;

	/* ***************** lock data from reading by user **************** */
	wdiag_set_valid(0);
	
	/* frame statistics */
	minic_get_stats(&tx, &rx);
	wdiags_write_cnts(tx, rx);

	/* local time */
	shw_pps_gen_get_time(&sec, &nsec);
	wdiags_write_time(sec, nsec);

	/* port state */
	wrpc_get_port_state(&ps, NULL);
	wdiags_write_port_state((wrc_global_link.link_up == NETIF_LINK_UP), (ps.locked ? 1 : 0));

	/* port PTP State (from ppsi)
	* see: ppsi/include/ppsi/ieee1588_types.h
	0  : none
	1  : PPS_INITIALIZING
	2  : PPS_FAULTY
	3  : PPS_DISABLED
	4  : PPS_LISTENING
	5  : PPS_PRE_MASTER
	6  : PPS_MASTER
	7  : PPS_PASSIVE
	8  : PPS_UNCALIBRATED
	9  : PPS_SLAVE
	*/
	wdiags_write_ptp_state((uint8_t)ppi->state);

	
	/* servo state (if slave)s */
	if (ptp_mode == WRC_MODE_SLAVE) {
		struct pp_servo *s = SRV(ppg->pp_instances);
		struct wr_servo_ext *wr_servo_ext = NULL;
		struct wrh_servo_t *wrh_servo = NULL;
		int32_t asym;
		int wr_mode;
		int32_t cur_setpoint_ps = 0;
		uint64_t mu = 0;

		asym = interval_to_picos(ppi_static.portDS->delayAsymmetry);
		wr_mode = (s->flags & PP_SERVO_FLAG_VALID) ? 1 : 0;

		wrh_servo = (ppi_static.protocol_extension == PPSI_EXT_WR && ppi_static.extState == PP_EXSTATE_ACTIVE) ?
			    (wrh_servo_t*) ppi_static.ext_data : NULL;

		if (wrh_servo) {
			wr_servo_ext = &((struct wr_data *)wrh_servo)->servo_ext;
			mu = pp_time_to_picos(&wr_servo_ext->rawDelayMM),
			cur_setpoint_ps = wrh_servo->cur_setpoint_ps;
		}

		/* see ppsi/include/hw-specific/wrh.h:
		0: WRH_UNINITIALIZED = 0,
		1: WRH_SYNC_TAI,
		2: WRH_SYNC_NSEC,
		3: WRH_SYNC_PHASE,
		4: WRH_TRACK_PHASE,
		5: WRH_WAIT_OFFSET_STABLE */
		wdiags_write_servo_state(wr_mode,
					 s->state,
					 mu,
					 pp_time_to_picos(&s->delayMS),
					 asym,
					 (int32_t) pp_time_to_picos(&s->offsetFromMaster),
					 cur_setpoint_ps,
					 ppi_static.servo->update_count);
	}

	/* Auxiliar channels (if any) */
	spll_get_num_channels(NULL, &n_out);
	if (n_out > 8) n_out = 8; /* hardware limit. */
	for (i = 0; i < n_out; i++) {
		aux_stat |= ((SPLL_AUX_SLAVE_LOCKED | SPLL_AUX_TRACKING_READY) & spll_get_aux_status(i).flags) << i;
	}
	wdiags_write_aux_state(aux_stat);

	/* temperature */
	if (HAS_TEMP_SENSORS) {
		temp = wrc_temp_get("pcb");
		wdiags_write_temp(temp);
	}

	/* **************** unlock data from reading by user  ************** */
	wdiag_set_valid(1);
	return 1;
}

