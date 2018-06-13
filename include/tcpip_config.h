#ifndef __TCPIP_CONFIG_H
#define __TCPIP_CONFIG_H

#include <stdint.h>

void tcpip_init(uint8_t mac_addr[]);
void tcpip_ip_addr(uint8_t *ip);
void tcpip_gateway_addr(uint8_t *gw);
void tcpip_subnet_addr(uint8_t *sn);
void tcpip_rx_dst_port(uint16_t port);
void tcpip_tx_src_port(uint16_t port);
void tcpip_tx_dst_port(uint16_t port);
void tcpip_set_hisIP(uint8_t *ip);
void tcpip_get_hisIP(uint8_t *ip);
void tcpip_set_hisMAC(uint8_t mac_addr[]);
void tcpip_rx_tcp_port(uint16_t port);
void tcpip_arp();

enum tcpip_status {
	TCPIP_ARP,
	TCPIP_OK,
};
extern enum tcpip_status tcpip_status;
extern uint8_t arp_count;

#endif
