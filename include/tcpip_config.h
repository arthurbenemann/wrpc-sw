#ifndef __TCPIP_CONFIG_H
#define __TCPIP_CONFIG_H

#include <stdint.h>

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

#define TCPIP_NULL 0
#define TCPIP_PRS 1
#define TCPIP_ARP 2
#define TCPIP_OK 3
uint8_t tcpip_status;

#endif
