/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>
#include <lib/ipv4.h>

#include "softpll_ng.h"
#include "shell.h"

void print_ip(void)
{
	unsigned char ip[wr_num_ports][4];
	char buf[20];
	int port;

	for (port = 0; port < wr_num_ports; port++) {
		getIP(ip[port], port);
		format_ip(buf, ip[port]);
		switch (ip_status[port]) {
		case IP_TRAINING:
			pp_printf("IP-address: in training\n");
			break;
		case IP_OK_BOOTP:
			pp_printf("IP-address: %s (from bootp)\n", buf);
			break;
		case IP_OK_STATIC:
			pp_printf("IP-address: %s (static assignment)\n", buf);
			break;
		}
	}
}

void decode_ip(const char *str, unsigned char *ip)
{
	int i, x;

	/* Don't try to detect bad input; need small code */
	for (i = 0; i < 4; ++i) {
		str = fromdec(str, &x);
		ip[i] = x;
		if (*str == '.')
			++str;
	}
}

char *format_ip(char *s, const unsigned char *ip)
{
	pp_sprintf(s, "%d.%d.%d.%d",
		   ip[0], ip[1], ip[2], ip[3]);
	return s;
}

static int cmd_ip(const char *args[])
{
	unsigned char ip[4];
	int port;

	if (!args[0] || !strcasecmp(args[0], "get")) {
		print_ip();
	} else if (!strcasecmp(args[0], "set") && args[1]) {
		if (args[2])
			port = atoi(args[2]);
		else
			port = 0;
		if (port > 1) return -EINVAL;
		ip_status[port] = IP_OK_STATIC;
		decode_ip(args[1], ip);
		setIP(ip, port);
		print_ip();
	} else {
		return -EINVAL;
	}
	return 0;
}

DEFINE_WRC_COMMAND(ip) = {
	.name = "ip",
	.exec = cmd_ip,
};
