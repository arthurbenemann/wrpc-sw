#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>
#include <lib/ipv4.h>
#include "tcpip_config.h"
#include "shell.h"

void tcpip_help()
{
	pp_printf("help\n");
}
static int cmd_tcpip(const char *args[])
{
	unsigned char ip[4];
	uint16_t port;

	if (!args[0])
		tcpip_help();
	else if (!strcasecmp(args[0], "tcp") && args[1]) {
		port = (uint16_t)args[1];
		tcpip_rx_tcp_port(port);
	} else if (!strcasecmp(args[0], "udp") && args[1] && args[2]) {
		if (!strcasecmp(args[1],"rxport"))
		{
			port = (uint16_t)args[2];
			tcpip_rx_dst_port(port);
		} else if (!strcasecmp(args[1], "txport")) {
			port = (uint16_t)args[2];
			tcpip_tx_dst_port(port);
		} else if (!strcasecmp(args[1], "txip")) {
			decode_ip(args[2], ip);
			tcpip_set_hisIP(ip);
		} else if (!strcasecmp(args[1], "txgw")) {
			decode_ip(args[2], ip);
			tcpip_gateway_addr(ip);
		} else if (!strcasecmp(args[1], "txsn")) {
			decode_ip(args[2], ip);
			tcpip_subnet_addr(ip);
		}
	} else {
		return -EINVAL;
	}
	return 0;
}

DEFINE_WRC_COMMAND(tcpip) = {
	.name = "tcpip",
	.exec = cmd_tcpip,
};
