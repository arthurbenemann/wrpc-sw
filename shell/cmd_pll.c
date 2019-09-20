/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "softpll_ng.h"
#include "shell.h"


void send_be32(uint32_t x)
{
	uint8_t str[4];
	str[0] = (x >> 24);
	str[1] = (x >> 16) & 0xff;
	str[2] = (x >> 8) & 0xff;
	str[3] = (x >> 0) & 0xff;
	console_uart_write_bytes(str, 4);
}

void send_spll_data( uint32_t *buf, int size )
{
	uint32_t cksum = 0;
	cksum += size;

	send_be32( size );
	int i;
	for( i = 0; i < size ;i++)
	{
		send_be32( buf[i] );
		cksum += buf[i];
	}

	send_be32(0xdeadbeef);
	//send_be32(cksum);
}

static int cmd_pll(const char *args[])
{
	int cur, tgt;

	if (!strcasecmp(args[0], "init")) {
		if (!args[3])
			return -EINVAL;
		spll_init(atoi(args[1]), atoi(args[2]), atoi(args[3]));
	} else if (!strcasecmp(args[0], "cl")) {
		if (!args[1])
			return -EINVAL;
		pp_printf("%d\n", spll_check_lock(atoi(args[1])));
	} else if (!strcasecmp(args[0], "stat")) {
		spll_show_stats();
	} else if (!strcasecmp(args[0], "sps")) {
		if (!args[2])
			return -EINVAL;
		spll_set_phase_shift(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gps")) {
		if (!args[1])
			return -EINVAL;
		spll_get_phase_shift(atoi(args[1]), &cur, &tgt);
		pp_printf("%d %d\n", cur, tgt);
	} else if (!strcasecmp(args[0], "start")) {
		if (!args[1])
			return -EINVAL;
		spll_start_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "stop")) {
		if (!args[1])
			return -EINVAL;
		spll_stop_channel(atoi(args[1]));
	} else if (!strcasecmp(args[0], "sdac")) {
		if (!args[2])
			return -EINVAL;
		spll_set_dac(atoi(args[1]), atoi(args[2]));
	} else if (!strcasecmp(args[0], "gdac")) {
		if (!args[1])
			return -EINVAL;
		pp_printf("%d\n", spll_get_dac(atoi(args[1])));
	} else if(!strcasecmp(args[0], "checkvco")) {
		check_vco_frequencies();
	} else if(!strcasecmp(args[0], "dbgdump"))
	{
		uint8_t str[5];

		phy_calibration_disable();

		str[0] = 0xca;
		str[1] = 0xfe;
		str[2] = 0xba;
		str[3] = 0xbe;
		console_uart_write_bytes(str, 4); // sync word
		console_uart_set_crlf_mode(0);
		int nt = 0;
		uint32_t buf[256];
		for(;;)
		{

			int c = console_getc();

			if( c == 0x1b)
				break;

			if( c == 'x')
			{
				nt = spll_get_debug_queue_samples( buf, 128 / 8, 1 );
				send_spll_data( buf, nt );
			}

			if( c == 'r' )
				send_spll_data( buf, nt );

		}

		console_uart_set_crlf_mode(1);


	}
	else
		return -EINVAL;

	return 0;
}

DEFINE_WRC_COMMAND(pll) = {
	.name = "pll",
	.exec = cmd_pll,
};
