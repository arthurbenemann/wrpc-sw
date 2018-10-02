/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 GSI (www.gsi.de)
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
#include "pps_gen.h"
#include "hw/memlayout.h"
#include "hw/etherbone-config.h"
#include "flash.h"
#include "tcpip_config.h"

enum ip_status ip_status[2] = {IP_TRAINING,IP_TRAINING};
static uint8_t myIP[2][4];
/* magic UDP is deadbeef */
const uint8_t magicUDP[4] ={0x62,0x65,0x65,0x66};

/* bootp: bigger buffer, UDP based */
static uint8_t __bootp_queue[2][512];
static struct wrpc_socket __static_bootp_socket[2] = {
	{.queue.buff = __bootp_queue[0],
	.queue.size = sizeof(__bootp_queue[0]),},
	{.queue.buff = __bootp_queue[1],
	.queue.size = sizeof(__bootp_queue[1]),}
};
static struct wrpc_socket *bootp_socket[2];

/* ICMP: smaller buffer */
static uint8_t __icmp_queue[2][128];
static struct wrpc_socket __static_icmp_socket[2] = {
	{.queue.buff = __icmp_queue[0],
	.queue.size = sizeof(__icmp_queue[0]),},
	{.queue.buff = __icmp_queue[1],
	.queue.size = sizeof(__icmp_queue[1]),}
};
static struct wrpc_socket *icmp_socket[2];

/* RDATE: even smaller buffer -- but we require 86. 96 is "even". */
static uint8_t __rdate_queue[96];
static struct wrpc_socket __static_rdate_socket = {
	.queue.buff = __rdate_queue,
	.queue.size = sizeof(__rdate_queue),
};
static struct wrpc_socket *rdate_socket;

/* remoteupdate: bigger buffer, UDP based */
static uint8_t __rmupdate_queue[340];
static struct wrpc_socket __static_rmupdate_socket = {
	.queue.buff = __rmupdate_queue,
	.queue.size = sizeof(__rmupdate_queue),
};
static struct wrpc_socket *rmupdate_socket;

/* syslog is selected by Kconfig, so we have weak aliases here */
void __attribute__((weak)) syslog_init(void)
{ }

int __attribute__((weak)) syslog_poll(void)
{ return 0; }

unsigned int ipv4_checksum(unsigned short *buf, int shorts)
{
	int i;
	unsigned int sum;

	sum = 0;
	for (i = 0; i < shorts; ++i)
		sum += ntohs(buf[i]);

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (~sum & 0xffff);
}

static void ipv4_init(void)
{
	struct wr_sockaddr saddr;
	int port;

	/* Bootp: use UDP engine activated by function arguments  */
	for (port=0; port < wr_num_ports; ++port) {
		bootp_socket[port] = ptpd_netif_create_socket(&__static_bootp_socket[0], NULL,
							PTPD_SOCK_UDP, 68 /* bootpc */, port);
	}

	/* time (rdate): UDP */
	rdate_socket = ptpd_netif_create_socket(&__static_rdate_socket, NULL,
					       PTPD_SOCK_UDP, 37 /* time */, 0);

	/* remote update (rmupdate): UDP */
	rmupdate_socket = ptpd_netif_create_socket(&__static_rmupdate_socket, NULL,
						PTPD_SOCK_UDP, 71 /* remote update */, 0);

	/* ICMP: specify raw (not UDP), with IPV4 ethtype */
	memset(&saddr, 0, sizeof(saddr));
	saddr.ethertype = htons(0x0800);
	for (port=0; port < wr_num_ports; ++port) {
		icmp_socket[port] = ptpd_netif_create_socket(&__static_icmp_socket[0], &saddr,
						       PTPD_SOCK_RAW_ETHERNET, 0, port);
	}
	syslog_init();
}

static int bootp_retry = 0;
static uint32_t bootp_tics;

/* receive bootp through the UDP mechanism */
static int bootp_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[400];
	int len, ret = 0;

	len = ptpd_netif_recvfrom(bootp_socket[0], &addr,
				  buf, sizeof(buf), NULL, 0);

	if (ip_status[0] != IP_TRAINING)
		return 0;

	if (len > 0)
		ret = process_bootp(buf, len);

	if (task_not_yet(&bootp_tics, TICS_PER_SECOND))
		return ret;

	len = prepare_bootp(&addr, buf, ++bootp_retry);
	ptpd_netif_sendto(bootp_socket[0], &addr, buf, len, 0, 0);
	return 1;
}

static int icmp_poll(int port)
{
	struct wr_sockaddr addr;
	uint8_t buf[128];
	int len;

	len = ptpd_netif_recvfrom(icmp_socket[port], &addr,
				  buf, sizeof(buf), NULL, port);
	if (len <= 0)
		return 0;
	if (ip_status[port] == IP_TRAINING)
		return 0;

	/* check the destination IP */
	if (check_dest_ip(buf, port))
		return 0;

	if ((len = process_icmp(buf, len, port)) > 0)
		ptpd_netif_sendto(icmp_socket[port], &addr, buf, len, 0, port);
	return 1;
}

static int rdate_poll(void)
{
	struct wr_sockaddr addr;
	uint64_t secs;
	uint32_t result;
	uint8_t buf[32];
	int len;

	len = ptpd_netif_recvfrom(rdate_socket, &addr,
				  buf, sizeof(buf), NULL, 0/* port */);
	if (len <= 0)
		return 0;

	/* check the destination IP */
	if (check_dest_ip(buf,0))
		return 0;

	shw_pps_gen_get_time(&secs, NULL);
	result = htonl((uint32_t)(secs + 2208988800LL));
	/* Magic above: $(date +%s --date="Jan 1 1900 00:00:00 UTC)" */

	len = UDP_END + sizeof(result);
	memcpy(buf + UDP_END, &result, sizeof(result));

	fill_udp(buf, len, NULL);
	ptpd_netif_sendto(rdate_socket, &addr, buf, len, 0, 0);
	return 1;
}

static int rmupdate_poll(void)
{
	struct wr_sockaddr addr;
	uint8_t buf[512];
	int len;
	uint32_t type;
	uint32_t data_addr;
	uint8_t* reg_addr;
	int data_size;
	int port;

	len = ptpd_netif_recvfrom(rmupdate_socket, &addr,
				  buf, sizeof(buf), NULL, 0);
	if (len <= 0)
		return 0;	


	/* check the destination IP */
	if (check_dest_ip(buf, 0))
		return 0;
	
	if (check_magic_udp(buf)<0)
	{
		// magic data error
		memset(buf+UDP_END, 0x00000001, 4);
		len = UDP_END + 4 + 64;
	}
	else if (check_magic_udp(buf)>0)
	{
		// udp checksum err
		memset(buf+UDP_END, 0xffff0000, 4);
		len = UDP_END + 4 + 64;
		return 0;
	}
	else 
	{
		type = (buf[UDP_END+6]<<8)+buf[UDP_END+7];
		switch(type)
		{
			case FLASH_ERASE:
				for (port = 0; port < wr_num_ports; ++port)
					wrc_ptp_run(0,port);
				data_addr = (buf[UDP_END+8]<<24)+(buf[UDP_END+9]<<16)+(buf[UDP_END+10]<<8)+buf[UDP_END+11];
				data_size = (buf[UDP_END+12]<<24)+(buf[UDP_END+13]<<16)+(buf[UDP_END+14]<<8)+buf[UDP_END+15];
				flash_erase(data_addr,data_size);
				memset(buf+UDP_END+12, 0x00000000, 4);
				len = UDP_END + 16 + 64;
			case FLASH_WRITE:
				data_addr = (buf[UDP_END+8]<<24)+(buf[UDP_END+9]<<16)+(buf[UDP_END+10]<<8)+buf[UDP_END+11];
				data_size = (buf[UDP_END+12]<<24)+(buf[UDP_END+13]<<16)+(buf[UDP_END+14]<<8)+buf[UDP_END+15];
				flash_write(data_addr,buf+UDP_END+16,data_size);
				memset(buf+UDP_END+12, 0x00000000, 4);
				len = UDP_END + 16 + 64 ;
				break;
			case FLASH_READ:
				data_addr = (buf[UDP_END+8]<<24)+(buf[UDP_END+9]<<16)+(buf[UDP_END+10]<<8)+buf[UDP_END+11];
				data_size = (buf[UDP_END+12]<<24)+(buf[UDP_END+13]<<16)+(buf[UDP_END+14]<<8)+buf[UDP_END+15];
				flash_read(data_addr,buf+UDP_END+16,data_size);
				len = UDP_END + 16 + data_size;
				break;
			case REG_WRITE:
				reg_addr = (buf[UDP_END+8]<<24)+(buf[UDP_END+9]<<16)+(buf[UDP_END+10]<<8)+buf[UDP_END+11];
				data_size = (buf[UDP_END+12]<<24)+(buf[UDP_END+13]<<16)+(buf[UDP_END+14]<<8)+buf[UDP_END+15];
				*(uint32_t *)reg_addr = (buf[UDP_END+16]<<24)+(buf[UDP_END+17]<<16)+(buf[UDP_END+18]<<8)+buf[UDP_END+19];
				memset(buf+UDP_END+16, 0x00000000, 4);
				len = UDP_END + 16 + 64;
				break;
			case REG_READ:
				reg_addr = (buf[UDP_END+8]<<24)+(buf[UDP_END+9]<<16)+(buf[UDP_END+10]<<8)+buf[UDP_END+11];
				data_size = (buf[UDP_END+12]<<24)+(buf[UDP_END+13]<<16)+(buf[UDP_END+14]<<8)+buf[UDP_END+15];
				memcpy(buf+UDP_END+16,reg_addr,data_size);
				len = UDP_END + 16 + data_size + 64;
				break;
			default:
				// type error
				memset(buf+UDP_END, 0x00000002, 4);
				len = UDP_END + 4 + 64;
		}
	}

	fill_udp(buf, len, NULL);
	ptpd_netif_sendto(rdate_socket, &addr, buf, len, 0, 0);
	return 1;
}

static int ipv4_poll(void)
{
	int ret = 0;

	if (link_status[0] == LINK_WENT_UP && ip_status[0] == IP_OK_BOOTP)
		ip_status[0] = IP_TRAINING;
	ret = bootp_poll();

	ret += icmp_poll(0);
	ret += icmp_poll(1);

	ret += rdate_poll();

	ret += rmupdate_poll();

	ret += syslog_poll();

	return ret != 0;
}

void getIP(unsigned char *IP, int port)
{
	memcpy(IP, myIP[port], 4);
}

DEFINE_WRC_TASK(ipv4) = {
	.name = "ipv4",
	.enable = &link_status[0],
	.init = ipv4_init,
	.job = ipv4_poll,
};

void setIP(unsigned char *IP, int port)
{
	// uint8_t tmp[4];
	// volatile unsigned int *eb_ip =
	//    (unsigned int *)(BASE_ETHERBONE_CFG + EB_IPV4);
	// unsigned int ip;
	// while (*eb_ip != ip)
	// 	*eb_ip = ip;

	memcpy(myIP[port], IP, 4);

	bootp_retry = 0;
}

/* Check the destination IP of the incoming packet */
int check_dest_ip(unsigned char *buf, int port)
{
	if (!buf)
		return -1;
	return memcmp(buf + IP_DEST, myIP[port], 4);
}

/* Check the magic number of the incoming remote update packet */
// 0 is ok, -1 has no packet, 1 checksum err
int check_magic_udp(unsigned char *buf)
{
	int i;
	unsigned int sum;
	unsigned int packet_size;
	sum = 0;
	if (memcmp(buf+UDP_END, magicUDP, 4)==0)
	{
		// pseudo header
		sum += (ntohs(buf[IP_SOURCE])<<8)+ntohs(buf[IP_SOURCE+1]);
		sum += (ntohs(buf[IP_SOURCE+2])<<8)+ntohs(buf[IP_SOURCE+3]);
		sum += (ntohs(buf[IP_DEST])<<8)+ntohs(buf[IP_DEST+1]);
		sum += (ntohs(buf[IP_DEST+2])<<8)+ntohs(buf[IP_DEST+3]);
		sum += ntohs(buf[IP_PROTOCOL]);
		packet_size = (ntohs(buf[UDP_LENGTH])<<8) + ntohs(buf[UDP_LENGTH+1]);
		sum += packet_size;
		// udp header 
		for (i=IP_END; (i <= UDP_END+packet_size-8); i=i+2)
		{
			sum += (ntohs(buf[i])<<8);
			sum += ntohs(buf[i+1]);
		}
		sum = (sum >> 16) + (sum & 0xffff);
		return (sum!=0xffff);
	}
	return -1;
}
