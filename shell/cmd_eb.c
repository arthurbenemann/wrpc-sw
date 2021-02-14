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
#include "dev/etherbone.h"
#include "shell.h"

#include "hw/etherbone-config.h"

static int cmd_eb(const char *args[])
{
	unsigned char ebip[4];
	unsigned char mac[6];
	uint32_t port;
	char ipbuf[20];
	char macbuf[32];

	if (!strcasecmp(args[0], "readip")) {
		eb_readIP(&ebip);
		format_ip(ipbuf, ebip);
		pp_printf("Etherbone IP: %s\n",ipbuf);
	} else if (!strcasecmp(args[0], "readmac")) {
		eb_readMAC(mac);
		format_mac(macbuf,mac);
		pp_printf("Etherbone MAC: %s\n",macbuf);	
	} else if (!strcasecmp(args[0], "readport")) {
		eb_readPort(&port);
		pp_printf("Etherbone port: %d\n",port);	
	} else if (!strcasecmp(args[0], "setip")) {
		decode_ip(args[1], ebip);
		eb_setIP(ebip);
		eb_readIP(&ebip);
		format_ip(ipbuf, ebip);
		pp_printf("Etherbone IP: %s\n",ipbuf);
	} else
		return -EINVAL;
	return 0;
}

DEFINE_WRC_COMMAND(pll) = {
	.name = "eb",
	.exec = cmd_eb,
};
