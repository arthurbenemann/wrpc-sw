/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>

#include <libdevmap.h>
#include "libertm.h"
#include "spll_debug.h"

static void wrpc_spll_recorder_help(char *prog)
{
	const char *mapping_help_str;

	mapping_help_str = dev_mapping_help();
	fprintf(stderr, "SoftPLL debug/recorder tool. \n");
	fprintf(stderr, "This dumps the real-time SPLL traces (error values/DAC drive/events) into stdout for the purpose of further analysis/plotting. \n");

	fprintf(stderr, "Usage: %s [options]\n", prog);
	fprintf(stderr, "%s", mapping_help_str);
	fprintf(stderr, "        -e <USB device for eRTM14>\n");
	fprintf(stderr, "The address offset must point to the base address of the WRCore (except the eRTM).\n");
	fprintf(stderr, "In the case of the eRTM14 board, just provide the UART/USB device through the -e option.\n");
}

static const char *dbg_source_to_string(int src)
{
	switch (src)
	{
	case SPLL_DBG_SRC_HELPER:
		return "helper";
	case SPLL_DBG_SRC_MAIN:
		return "main";
	case SPLL_DBG_SRC_AUX(0):
		return "aux0";
	case SPLL_DBG_SRC_AUX(1):
		return "aux1";
	case SPLL_DBG_SRC_AUX(2):
		return "aux2";
	case SPLL_DBG_SRC_AUX(3):
		return "aux3";
	case SPLL_DBG_SRC_EXT:
		return "ext";
	default:
		return "<unknown?>";
	}
}

static const char *dbg_signal_to_string(int src)
{
	switch (src)
	{
	case SPLL_DBG_SIGNAL_ERR:
		return "err";
	case SPLL_DBG_SIGNAL_Y:
		return "y";
	case SPLL_DBG_SIGNAL_PERIOD:
		return "period";
	case SPLL_DBG_SIGNAL_REF:
		return "ref";
	case SPLL_DBG_SIGNAL_TAG:
		return "tag";
	case SPLL_DBG_SIGNAL_SAMPLE_ID:
		return "sample";
	case SPLL_DBG_SIGNAL_TIME_MS:
		return "time_ms";
	case SPLL_DBG_SIGNAL_PHASE_CURRENT:
		return "phase_current";
	case SPLL_DBG_SIGNAL_PHASE_TARGET:
		return "phase_target";
	default:
		return "<unknown?>";
	}
}

static const char *dbg_event_to_string(int src)
{
	switch (src)
	{
	case SPLL_DBG_EVT_GAIN_SWITCH:
		return "gain-switch";
	case SPLL_DBG_EVT_LOCK_ACQUIRED:
		return "lock-acquired";
	case SPLL_DBG_EVT_LOCK_LOSS:
		return "lock-lost";
	case SPLL_DBG_EVT_START:
		return "start";
	default:
		return "<unknown?>";
	}
}

static int32_t signext32(uint32_t in, int bit)
{
	uint32_t mask = ~((1 << bit) - 1);
	if (in & (1 << bit))
		return in | mask;
	else
		return in;
}

static int prev_src = -1;
static volatile int kill_acquisition = 0;

void sighandler(int sig)
{
	fprintf(stderr, "Signal caught: %d\n", sig);
	kill_acquisition = 1;
}

void dump_debug_data(const uint32_t *buf, size_t size)
{
	while (size--)
	{
		uint32_t x = *buf++;

		int sig = SPLL_DBG_EXTRACT_SIGNAL(x);
		int src = SPLL_DBG_EXTRACT_SOURCE(x);
		uint32_t value_raw = SPLL_DBG_EXTRACT_VALUE(x);
		int32_t value;

		switch (sig)
		{
		case SPLL_DBG_SIGNAL_ERR:
			value = signext32(value_raw, 23);
			break;
		default:
			value = value_raw;
		};

		if (prev_src != src)
		{
			printf("%s ", dbg_source_to_string(src));
			prev_src = src;
		}

		if (sig == SPLL_DBG_SIGNAL_EVENT)
		{
			printf(" event=%s", dbg_event_to_string(value));
		}

		printf("%s=%d ", dbg_signal_to_string(sig),
			   value);

		if (SPLL_DBG_IS_LAST_RECORD(x))
		{
			printf("\n");
			prev_src = -1;
		}
	}
}

void readout_ertm14(const char *uart_device)
{
	struct ertm_status *handle = ertm_init(uart_device);

	if (handle == NULL)
	{
		fprintf(stderr, "could not open %s\n", uart_device);
		exit(1);
	}

	int r = ertm_configure_spll_debug_dump(handle, 1, 20);
	if (r)
		perror("ertm_configure_spll_debug_dump()");

	for (;;)
	{
		uint32_t buf[16384];
		size_t buf_size = 16384;
		int r = ertm_read_spll_debug_data(handle, buf, &buf_size);
		if (r >= 0)
		{
			dump_debug_data(buf, buf_size);
		}

		if (kill_acquisition)
			break;
	}

	r = ertm_configure_spll_debug_dump(handle, 0, 0);
	if (r)
		perror("ertm_configure_spll_debug_dump()");

	ertm_exit(handle);
	fprintf(stderr, "Exiting...\n");
}

void readout_direct(struct mapping_desc *spll)
{
}

int main(int argc, char *argv[])
{
	char *uart_dev = NULL;
	struct mapping_args *map_args;
	struct mapping_desc *spll = NULL;
	int c;

	/* Parse specific args */
	while ((c = getopt(argc, argv, "e:h")) != -1)
	{
		switch (c)
		{
		case 'e':
			uart_dev = strdup(optarg);
			break;
		case 'h':
			wrpc_spll_recorder_help(argv[0]);
			return 0;
		case '?':
			break;
		}
	}

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	if (uart_dev)
	{
		readout_ertm14(uart_dev);
	}
	else
	{
		map_args = dev_parse_mapping_args(&argc, argv);
		if (!map_args)
		{
			wrpc_spll_recorder_help(argv[0]);
			return 1;
		}

		spll = dev_map(map_args, getpagesize());
		if (!spll)
		{
			fprintf(stderr, "%s: SPLL mapping failed: %s\n", argv[0],
					strerror(errno));
			return -1;
		}
		readout_direct(spll);
		dev_unmap(spll);
	}

	return 0;
}
