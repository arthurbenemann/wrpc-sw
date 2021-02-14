/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <shell.h>
#include <dev/storage.h>
#include <dev/endpoint.h>
#include <ppsi/ppsi.h>
#include "wr-api.h"

static int cmd_devmem(const char *args[])
{
	uint32_t *addr, value;

	if (!args[0]) {
		pp_printf("devmem: use: \"devmem <address> [<value>]\"\n");
		return 0;
	}
	fromhex(args[0], (void *)&addr);
	if (args[1]) {
		fromhex(args[1], (void *)&value);
		*addr = value;
	} else {
		pp_printf("%08x = %08x\n", (int)addr, *addr);
	}
	return 0;
}

DEFINE_WRC_COMMAND(devmem) = {
	.name = "devmem",
	.exec = cmd_devmem,
};

extern struct pp_instance ppi_static[wr_num_ports];

static int cmd_delays(const char *args[])
{
	int tx, rx;
	int port;
	struct wr_data *wrp;
	struct wr_servo_state *s;

	if (args[0] && !args[1]) {
		pp_printf("delays: use: \"delays [<txdelay> <rxdelay>] [port]\"\n");
		return 0;
	}
	if (args[1]) {
		fromdec(args[0], &tx);
		fromdec(args[1], &rx);
		if (args[2])
			port = atoi(args[2]);
		else
			port = 0;
		if (port > 1) return -1;

		sfp_deltaTx[port] = tx;
		sfp_deltaRx[port] = rx;
		wrp = (void *)(ppi_static[port].ext_data);
		s = &wrp->servo_state;
		/* Change the active value too (add bislide here) */
		s->delta_tx_m = tx;
		s->delta_rx_m = rx + ep_get_bitslide(port);
	} else {
		for (port = 0; port < wr_num_ports; ++port)
		{
			pp_printf("port %d: tx: %i   rx: %i\n", port, sfp_deltaTx[port], sfp_deltaRx[port]);
		}
	}
	return 0;
}

DEFINE_WRC_COMMAND(delays) = {
	.name = "delays",
	.exec = cmd_delays,
};
