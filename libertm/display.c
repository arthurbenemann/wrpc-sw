#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include "libertm.h"
#include "private.h"
#include <time.h>

static char *state_literal[] = {
	[ERTM_RF_OUT_ON] = "on",
	[ERTM_RF_OUT_OFF] = "off",
	[ERTM_RF_OUT_MONITOR] = "monitor",
};

/* FIXME: all these are repeated, same as above */
static double ampl_factor_to_float(uint8_t ampl_factor)
{
	return ampl_factor/256.0;
}
void display_dds_state(struct ertm14_dds_state *dds1,
			struct ertm14_dds_state *dds2)
{
	int i;

	printf("LO ftw: %08x (%7.3fMHz)%7c", dds1->ftw, (1000.0 * dds1->ftw) / (1L<<32), ' ');
	printf(" | ");
	printf("REF ftw: %08x (%7.3fMHz)%7c", dds2->ftw, (1000.0 * dds2->ftw) / (1L<<32), ' ');
	printf("\n");
	printf("LO level adjust: %6.4f (%3d/256)%3c", ampl_factor_to_float(dds1->ampl_factor), dds1->ampl_factor, ' ');
	printf(" | ");
	printf("REF level adjust: %6.4f (%3d/256)%3c", ampl_factor_to_float(dds2->ampl_factor), dds2->ampl_factor, ' ');
	printf("\n");
	printf("LO pll_out_power: %5.3f dBm%9c",  dds1->amp_power/100.0, ' ');	/* register in mBm, not mdBm! */
	printf(" | ");
	printf("REF pll_out_power: %5.3f dBm%9c",  dds2->amp_power/100.0, ' ');	/* ditto */
	printf("\n");
	printf("LO sync_state: %4s%17c", ertm_sync_states[dds1->sync_state].label, ' ');
	printf(" | ");
	printf("REF sync_state: %4s%17c", ertm_sync_states[dds2->sync_state].label, ' ');
	printf("\n");
	for (i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++) {
		printf("LO%02d:   pow: %5.3f dBm  st:%-8s",
		    i, dds1->out_power[i]/100.0, state_literal[dds1->out_state[i]]);
		printf(" | ");
		printf("REF%02d: pow: %5.3f dBm  st:%-8s",
		    i, dds2->out_power[i]/100.0, state_literal[dds2->out_state[i]]);
		printf("\n");
	}
}

void display_ertm_clkab(struct ertm14_board_state *bs)
{
	int i;
	for (i = ERTM_CLKAB_MIN_CH; i <= ERTM_CLKAB_MAX_CH; i++) {
		char *aonoff = (bs->clka_enable_mask & (1<<i)) ? "on " : "off";
		char *bonoff = (bs->clkb_enable_mask & (1<<i)) ? "on " : "off";
		char *async = ertm_sync_states[bs->clka_sync_state[i]].label;
		char *bsync = ertm_sync_states[bs->clkb_sync_state[i]].label;
		double a_mhz = bs->clka_freq_hz[i] / 1.0e6;
		double b_mhz = bs->clkb_freq_hz[i] / 1.0e6;
		printf("CLKA%02d: %3s %4s %6.1f MHz", i, aonoff, async, a_mhz);
		printf("     | ");
		printf("CLKB%02d: %3s %4s %6.1f MHz", i, bonoff, bsync, b_mhz);
		printf("\n");
	}
}

void display_ertm_state(struct ertm14_board_state *bs)
{
	printf("DDS: -------------------------------------------------------------------\n");
	display_dds_state(&bs->lo, &bs->ref);
	printf("CLKAB: -----------------------------------------------------------------\n");
	display_ertm_clkab(bs);
	printf("------------------------------------------------------------------------\n");
}

void display_wrc_diags(struct ertm_wr_status *diags)
{
	char fmt[] = "%-38s: 0x%08x\n";

	printf(fmt, "Version register", diags->VER);
	printf(fmt, "Ctrl", diags->CTRL);
	printf(fmt, "servo status", diags->WDIAG_SSTAT);
	printf(fmt, "Port status", diags->WDIAG_PSTAT);
	printf(fmt, "PTP state", diags->WDIAG_PTPSTAT);
	printf(fmt, "AUX state", diags->WDIAG_ASTAT);
	printf(fmt, "Tx PTP Frame cnts", diags->WDIAG_TXFCNT);
	printf(fmt, "Rx PTP Frame cnts", diags->WDIAG_RXFCNT);
	printf(fmt, "WRPC Diag:local time [msb of s]", diags->WDIAG_SEC_MSB);
	printf(fmt, "local time [lsb of s]", diags->WDIAG_SEC_LSB);
	printf(fmt, "local time [ns]", diags->WDIAG_NS);
	printf(fmt, "Round trip (mu) [msb of ps]", diags->WDIAG_MU_MSB);
	printf(fmt, "Round trip (mu) [lsb of ps]", diags->WDIAG_MU_LSB);
	printf(fmt, "Master-slave delay (dms) [msb of ps]", diags->WDIAG_DMS_MSB);
	printf(fmt, "Master-slave delay (dms) [lsb of ps]", diags->WDIAG_DMS_LSB);
	printf(fmt, "Total link asymmetry [ps]", diags->WDIAG_ASYM);
	printf(fmt, "Clock offset (cko) [ps]", diags->WDIAG_CKO);
	printf(fmt, "Phase setpoint (setp) [ps]", diags->WDIAG_SETP);
	printf(fmt, "Update counter (ucnt)", diags->WDIAG_UCNT);
	printf(fmt, "Board temperature [C degree]", diags->WDIAG_TEMP);
	
}

/* pulled from wrpc_diags.c */
static void print_servo_status(uint32_t val)
{
	static char *sstat_str[] = {
		"Not initialized",
		"Sync ns",
		"Sync TAI",
		"Sync phase",
		"Track phase",
		"Wait offset stable",
	};

	printf("servo status:\t\t%s\n",
		sstat_str[val >> WRC_DIAGS_WDIAG_SSTAT_SERVOSTATE_SHIFT]);
}

static void print_port_status(uint32_t val)
{
	static int nbits = 2;
	static char *pstat_str[][2] = {
		//bit = 0     	 	bit = 1
		{"Link down", 		"Link up",},
		{"PLL not locked",	"PLL locked",},
	};
	int i, idx;

	printf("Port status:\t\t");
	for (i = 0; i < nbits; ++i) {
		idx = (val & (1 << i)) ? 1 : 0;
		printf("%s, ", pstat_str[i][idx]);
	}
	printf("\n");
}

static void print_ptp_state(uint32_t val)
{
	static char *ptpstat_str[] = {
		"None",
		"PPS initializing",
		"PPS faulty",
		"disabled",
		"PPS listening",
		"PPS pre-master",
		"PPS master",
		"PPS passive",
		"PPS uncalibrated",
		"PPS slave",
	};

	printf("PTP state:\t\t");
	if (val <= 9)
		printf("%s", ptpstat_str[val]);
	else if (val >= 100 && val <= 116)
		printf("WR STATES(see ppsi/ieee1588_types.h): %d", val);
	else
		printf("Unknown");
	printf("\n");
}

static void print_aux_state(uint32_t val)
{
	int nch = 8; //should be retrieved from a register
	int i;

	printf("Aux state:\t\t");
	for (i = 0; i < nch; i++) {
		if (val & (1 << i))
			printf("ch%d:enabled ", i);
	}
	printf("\n");
}

static void print_tx_frame_count(uint32_t val)
{
	printf("TX frame count:\t\t%d\n", val);
}

static void print_rx_frame_count(uint32_t val)
{
	printf("RX frame count:\t\t%d\n", val);
}

static void print_local_time(uint32_t sec_msw, uint32_t sec_lsw, uint32_t ns)
{
	uint64_t sec = (uint64_t)(sec_msw) << 32 | sec_lsw;
//	printf("TAI time:\t\t %" PRIu64 "sec %d nsec\n",
//		sec, ns);
	printf("TAI time:\t\t%s", ctime((time_t *)&sec));
}

static void print_roundtrip_time(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	printf("Round trip time:\t%" PRIu64 " ps\n", val);
}

static void print_master_slave_delay(uint32_t msw, uint32_t lsw)
{
	uint64_t val = (uint64_t)(msw) << 32 | lsw;
	printf("Master slave delay:\t%" PRIu64 " ps\n", val);
}

static void print_link_asym(uint32_t val)
{
	printf("Total Link asymmetry:\t%d ps\n", val);
}

static void print_clock_offset(uint32_t val)
{
	printf("Clock offset:\t\t%d ps\n", val);
}

static void print_phase_setpoint(uint32_t val)
{
	printf("Phase setpoint:\t\t%d ps\n", val);
}

static void print_update_counter(uint32_t val)
{
	printf("Update counter:\t\t%d\n", val);
}

static void print_board_temp(uint32_t val)
{
	 printf("temp:\t\t\t%2d C\n", val);
}

void display_wrc_diags_cooked(struct ertm_wr_status *diags)
{
	char fmt[] = "%-20s\t0x%08x\n";

	printf(fmt, "Version register", diags->VER);
	printf(fmt, "Ctrl", diags->CTRL);
	print_servo_status(diags->WDIAG_SSTAT);
	print_port_status(diags->WDIAG_PSTAT);
	print_ptp_state(diags->WDIAG_PTPSTAT);
	print_aux_state(diags->WDIAG_ASTAT);
	print_tx_frame_count(diags->WDIAG_TXFCNT);
	print_rx_frame_count(diags->WDIAG_RXFCNT);
	print_local_time(diags->WDIAG_SEC_MSB, diags->WDIAG_SEC_LSB, diags->WDIAG_NS);
	print_roundtrip_time(diags->WDIAG_MU_MSB, diags->WDIAG_MU_LSB);
	print_master_slave_delay(diags->WDIAG_DMS_MSB, diags->WDIAG_DMS_LSB);
	print_link_asym(diags->WDIAG_ASYM);
	print_clock_offset(diags->WDIAG_CKO);
	print_phase_setpoint(diags->WDIAG_SETP);                                     
	print_update_counter(diags->WDIAG_UCNT);
	print_board_temp(diags->WDIAG_TEMP);                                     
}

static const char *source_name(int sync_source)
{
	switch (sync_source) {
	case ERTM14_SYNC_SOURCE_NONE:
		return "none";
	case ERTM14_SYNC_SOURCE_PPS:
		return "PPS";
	case ERTM14_SYNC_SOURCE_RF_TRIGGER:
		return "RF_TRIGGER";
	default:
		return NULL;
	};
};

void display_nco_status(struct ertm_nco_status *nco)
{
	struct ertm_nco_reset *lo  = &nco->lo;
	struct ertm_nco_reset *ref = &nco->ref;

	printf(
	    "%-3s DDS Sync Source:   %10s  %-3s DDS Sync Source:  %10s\n"
	    "%-3s DDS Sync Triggers: %10d  %-3s DDS Syn Triggers: %10d\n"
	    "RX message count:\t%d\n",
		"LO ", source_name(lo->sync_source),
		"REF", source_name(ref->sync_source),
		"LO ", lo->reset_count, "REF", ref->reset_count,
		lo->rx_count);
}

void mac_to_str(uint64_t mac, char *dst)
{
	sprintf(dst,
		"%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
		(mac >> 40) & 0xff,
		(mac >> 32) & 0xff,
		(mac >> 24) & 0xff,
		(mac >> 16) & 0xff,
		(mac >>  8) & 0xff,
		 mac        & 0xff);
}

void display_version_info(struct ertm_board_info *bi)
{
	char mac[20];

	mac_to_str(bi->ertm14_mac1, mac);
	printf(
	"ERTM14: Serial No:      %s\n"
	"ERTM14: MMC FW Version: %s\n"
	"ERTM15: Serial No:      %s\n"
	"ERTM15: MMC FW Version: %s\n"
	"ERTM14: MAC:            %s\n"
	"WRPCSW: commit:     %s\n"
	"WRPCSW: build date: %s %s\n"
	"WRPCSW: build by    %s\n",
		bi->ertm14_serial, bi->ertm14_firmware_version,
		bi->ertm15_serial, bi->ertm15_firmware_version,
		mac,
		bi->wrpc_sw_commit_id,
		bi->wrpc_sw_build_date,
		bi->wrpc_sw_build_time,
		bi->wrpc_sw_build_by);
	printf("FPGA buildinfo:\n\tcommit [%s]\n\tbuild date: [%s]\n",
		bi->firmware_metadata.source_id,
		bi->firmware_metadata.vendor_uuid);
};

void display_fpga_buildinfo(struct ertm_board_info *bi)
{
	printf("FPGA buildinfo:\n%s\n",
		bi->firmware_metadata.fpga_buildinfo_text);
};

void display_temperatures(struct ertm_temperatures *t)
{
	printf(
	    "%-10s %3gC\n" "%-10s %3gC\n" "%-10s %3gC\n"
	    "%-10s %3gC\n" "%-10s %3gC\n" "%-10s %3gC\n"
	    "%-10s %3gC\n" "%-10s %3gC\n" "%-10s %3gC\n"
	    "%-10s %3gC\n" "%-10s %3gC\n" "%-10s %3gC\n",
		"FPGA: ", t->fpga,
		"PSU14: ", t->power_supplies14,
		"PSU15 ", t->power_supplies15,
		"LO DDS: ", t->dds_lo,
		"REF DDS: ", t->dds_ref,
		"LO AMP: ", t->lo_amp,
		"REF AMP: ", t->ref_amp,
		"LTC6150: ", t->ltc6150,
		"OCXO1: ", t->ocxo_near,
		"OCXO2: ", t->ocxo_under,
		"CLKA: ", t->clka,
		"CLKB: ", t->clkb);
};

void display_voltages(struct ertm_voltages *v)
{		/* volts SVP */
	printf(
	    "%-12s %6.2fV\n" "%-12s %6.2fV\n" "%-12s %6.2fV\n"
	    "%-12s %6.2fV\n" "%-12s %6.2fV\n" "%-12s %6.2fV\n"
	    "%-12s %6.2fV\n" "%-12s %6.2fA\n",
		    "ERTM14 12V: ", v->p12v_ertm14,
		    "ERTM14 3V3: ", v->p3v3_ertm14,
		    "ERTM15 12V: ", v->p12v_ertm15,
		    "ERTM15 3V3: ", v->p3v3_ertm15,
		    "OCXO VOLT: ",  v->pocxo,
		    "LO 9V: ", 	    v->p9v0_lo,
		    "REF 9V: ",	    v->p9v0_ref,
		    "OCXO CURR: ",  v->ocxo_curr);
};

