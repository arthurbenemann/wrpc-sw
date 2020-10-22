/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <irq.h>
#include <shell.h>
#include <storage.h>
#include <endpoint.h>
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

static int cmd_multiread(const char *args[])
{
	volatile uint32_t *addr;
	uint32_t val, t0, t1;
	int count, i;

	if (!args[1]) {
		pp_printf("multiread: use: \"mr <0xaddress> <0xcount>\"\n");
		return 0;
	}
	fromhex(args[0], (void *)&addr);
	fromhex(args[1], (void *)&count);
	t0 = timer_get_tics();
	for (i = 0; i < count; i++)
		val = *addr;
	t1 = timer_get_tics();
	pp_printf("%i reads: %i tics\n", count, t1 - t0);
	return 0;
}

DEFINE_WRC_COMMAND(multiread) = {
	.name = "mr",
	.exec = cmd_multiread,
};

static int cmd_irq(const char *args[])
{
	if (!args[0]) {
		pp_printf("irq: use: \"irq on|off\"\n");
		return 0;
	}
	if (args[0][1] == 'f') {
		disable_irq();
		pp_printf("interrupts are not off\n");
	} else {
		enable_irq();
		pp_printf("interrupts are not on\n");
	}
	return 0;
}

DEFINE_WRC_COMMAND(irq) = {
	.name = "irq",
	.exec = cmd_irq,
};

extern struct pp_instance ppi_static;

static int cmd_delays(const char *args[])
{
	int tx, rx;
	struct wr_data *wrp = (void *)(ppi_static.ext_data);
	struct wr_servo_state *s = &wrp->servo_state;

	if (args[0] && !args[1]) {
		pp_printf("delays: use: \"delays [<txdelay> <rxdelay>]\"\n");
		return 0;
	}
	if (args[1]) {
		fromdec(args[0], &tx);
		fromdec(args[1], &rx);
		sfp_deltaTx = tx;
		sfp_deltaRx = rx;
		/* Change the active value too (add bislide here) */
		s->delta_tx_m = tx;
		s->delta_rx_m = rx + ep_get_bitslide();
	} else {
		pp_printf("tx: %i   rx: %i\n", sfp_deltaTx, sfp_deltaRx);
	}
	return 0;
}

DEFINE_WRC_COMMAND(delays) = {
	.name = "delays",
	.exec = cmd_delays,
};
