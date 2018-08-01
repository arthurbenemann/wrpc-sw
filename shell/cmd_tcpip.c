#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>
#include <lib/ipv4.h>
#include "tcpip_config.h"
#include "shell.h"

static int cmd_tcpip(const char *args[])
{
	unsigned char ip[4];
	uint16_t port;

	if (!args[0])
		return -EINVAL;
	else if (!strcasecmp(args[0], "tcp") && args[1] && args[2]) {
		if (!strcasecmp(args[1],"rxp"))
		{
			port = atoi(args[2]);
			tcpip_rx_tcp_port(port);
		} 
	} else if (!strcasecmp(args[0], "udp") && args[1] && args[2]) {
		if (!strcasecmp(args[1],"rxp"))
		{
			port = atoi(args[2]);
			tcpip_rx_dst_port(port);
		} else if (!strcasecmp(args[1], "txp")) {
			port = atoi(args[2]);
			tcpip_tx_dst_port(port);
		} 
	} else if (!strcasecmp(args[0], "dstip")) {
			decode_ip(args[1], ip);
			tcpip_set_hisIP(ip);
	} else if (!strcasecmp(args[0], "gateway")) {
			decode_ip(args[1], ip);
			tcpip_gateway_addr(ip);
		} else if (!strcasecmp(args[0], "subnet")) {
			decode_ip(args[1], ip);
			tcpip_subnet_addr(ip);
		} 
	else {
		return -EINVAL;
	}
	return 0;
}

DEFINE_WRC_COMMAND(tcpip) = {
	.name = "tcpip",
	.exec = cmd_tcpip,
};
