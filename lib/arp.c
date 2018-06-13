/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 GSI (www.gsi.de)
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include <wrpc.h>
#include <string.h>

#include "endpoint.h"
#include "ipv4.h"
#include "ptpd_netif.h"
#include "arp.h"
#include "tcpip_config.h"

static uint8_t __arp_queue[128];
static struct wrpc_socket __static_arp_socket = {
	.queue.buff = __arp_queue,
	.queue.size = sizeof(__arp_queue),
};
static struct wrpc_socket *arp_socket;

static void arp_init(void)
{
	struct wr_sockaddr saddr;

	/* Configure socket filter */
	memset(&saddr, 0, sizeof(saddr));
	// memset(&saddr.mac, 0xFF, 6);	/* Broadcast */
	saddr.ethertype = htons(0x0806);	/* ARP */

	arp_socket = ptpd_netif_create_socket(&__static_arp_socket, &saddr,
						PTPD_SOCK_RAW_ETHERNET, 0);
}

static int process_arp(uint8_t * buf, int len)
{
	uint8_t hisMAC[6];
	uint8_t hisIP[4];
	uint8_t myIP[4];

	if (len < ARP_END)
		return 0;

	/* Is it ARP request targetting our IP? */
	if (buf[ARP_OPER + 0] != 0)
		return 0;

	getIP(myIP);
	if ( ((buf[ARP_OPER + 1] != 1)||memcmp(buf + ARP_TPA, myIP, 4)) == 0 )
	{		
		memcpy(hisMAC, buf + ARP_SHA, 6);
		memcpy(hisIP, buf + ARP_SPA, 4);

		// ------------- ARP ------------
		// HW ethernet
		buf[ARP_HTYPE + 0] = 0;
		buf[ARP_HTYPE + 1] = 1;
		// proto IP
		buf[ARP_PTYPE + 0] = 8;
		buf[ARP_PTYPE + 1] = 0;
		// lengths
		buf[ARP_HLEN] = 6;
		buf[ARP_PLEN] = 4;
		// Response
		buf[ARP_OPER + 0] = 0;
		buf[ARP_OPER + 1] = 2;
		// my MAC+IP
		get_mac_addr(buf + ARP_SHA);
		memcpy(buf + ARP_SPA, myIP, 4);
		// his MAC+IP
		memcpy(buf + ARP_THA, hisMAC, 6);
		memcpy(buf + ARP_TPA, hisIP, 4);

		return ARP_END;
	}
	
	tcpip_get_hisIP(hisIP);
	if ( ((buf[ARP_OPER + 1] != 2)||memcmp(buf + ARP_SPA, hisIP, 4)) == 0 )
	{
		memcpy(hisMAC, buf + ARP_SHA, 6);
		tcpip_set_hisMAC(hisMAC);
		tcpip_get_hisMAC(hisMAC);
		tcpip_status = TCPIP_OK;
	}

	return 0;
}

static int arp_poll(void)
{
	uint8_t buf[ARP_END + 100];
	struct wr_sockaddr addr;
	int len;

	if (ip_status == IP_TRAINING)
		return 0;		/* can't do ARP w/o an address... */
    
    if ((tcpip_status == TCPIP_ARP) && arp_count < 200)
    {
    	tcpip_arp();
    	arp_count++;
    }
	if ((len = ptpd_netif_recvfrom(arp_socket,
					&addr, buf, sizeof(buf), 0)) > 0) {
		if ((len = process_arp(buf, len)) > 0)
			ptpd_netif_sendto(arp_socket, &addr, buf, len, 0);
		return 1;
	}
	return 0;
}

void send_arp(uint8_t * hisIP)
{
	uint8_t buf[ARP_END + 100];
	struct wr_sockaddr addr;
	/* Configure socket filter */
	memset(&addr.mac, 0xFF, 6);  /* Broadcast */
	// ------------- ARP ------------
	// HW ethernet
	buf[ARP_HTYPE + 0] = 0;
	buf[ARP_HTYPE + 1] = 1;
	// proto IP
	buf[ARP_PTYPE + 0] = 8;
	buf[ARP_PTYPE + 1] = 0;
	// lengths
	buf[ARP_HLEN] = 6;
	buf[ARP_PLEN] = 4;
	// request
	buf[ARP_OPER + 0] = 0;
	buf[ARP_OPER + 1] = 1;

	get_mac_addr(buf + ARP_SHA);
	getIP(buf + ARP_SPA);
	memset(buf + ARP_THA, 0x00, 6);  /* Broadcast */
	memcpy(buf + ARP_TPA, hisIP, 4);
	return (ptpd_netif_sendto(arp_socket, &addr, buf, ARP_END+10, 0));
}

DEFINE_WRC_TASK(arp) = {
	.name = "arp",
	.enable = &link_status,
	.init = arp_init,
	.job = arp_poll,
};
