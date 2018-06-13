#include <wrc.h>
#include <wrpc.h>
#include <string.h>

#include "endpoint.h"
#include "arp.h"
#include "hw/tcpip-config.h"
#include "tcpip_config.h"

enum tcpip_status tcpip_status;
uint8_t arp_count=0;

void tcpip_init(uint8_t mac_addr[])
{
  volatile unsigned int *tcpip_tmp;
  uint32_t tmp=0;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_MAC_HIGH16);
  tmp = ((uint32_t) mac_addr[0] << 8)
      | ((uint32_t) mac_addr[1]);
  *tcpip_tmp = tmp;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_MAC_LOW32);
  tmp = ((uint32_t) mac_addr[2] << 24)
      | ((uint32_t) mac_addr[3] << 16)
      | ((uint32_t) mac_addr[4] << 8)
      | ((uint32_t) mac_addr[5]);
  *tcpip_tmp = tmp;
  
  // default udp tx dst/src port
  tcpip_tx_dst_port(2000);
  tcpip_tx_src_port(2000);
}

void tcpip_ip_addr(uint8_t *ip)
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_IP_ADDR), ip, 4);
}

void tcpip_gateway_addr(uint8_t *gw)
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_GATEWAY), gw, 4);
}

void tcpip_subnet_addr(uint8_t *sn)
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_SUBNET_MASK), sn, 4);
}

void tcpip_rx_dst_port(uint16_t port)
{
  volatile unsigned int *rdp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_UDP_RX_PORT);  
  *rdp = 0x10000 + (uint32_t)port; // 0x10000 is used to enable setting rx dst port
}

void tcpip_tx_src_port(uint16_t port)
{
  volatile unsigned int *tsp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_SRC_PORT);  
  *tsp = (uint32_t)port;
}

void tcpip_tx_dst_port(uint16_t port)
{
  volatile unsigned int *tdp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_PORT);  
  *tdp = (uint32_t)port;
}

void tcpip_set_hisIP(uint8_t *ip)
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_IP), ip, 4);
  tcpip_status = TCPIP_ARP;
  arp_count = 0;
}

void tcpip_get_hisIP(uint8_t *ip)
{
  memcpy(ip, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_IP), 4);
}

void tcpip_set_hisMAC(uint8_t mac_addr[])
{
  volatile unsigned int *tcpip_tmp;
  uint32_t tmp=0;
  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_MAC_HIGH16);
  tmp = ((uint32_t) mac_addr[0] << 8)
  | ((uint32_t) mac_addr[1]);
  *tcpip_tmp = tmp;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_MAC_LOW32);
  tmp = ((uint32_t) mac_addr[2] << 24)
  | ((uint32_t) mac_addr[3] << 16)
  | ((uint32_t) mac_addr[4] << 8)
  | ((uint32_t) mac_addr[5]);
  *tcpip_tmp = tmp;
}

void tcpip_get_hisMAC(uint8_t mac_addr[])
{
  memcpy(mac_addr, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_HIGH16+2), 2);
  memcpy(mac_addr+2, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_LOW32), 4);
}

void tcpip_rx_tcp_port(uint16_t port)
{
  volatile unsigned int *rtp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_TCP_LOCAL_PORT);  
  *rtp = (uint32_t)port;
}

void tcpip_arp()
{
  uint8_t * ip;
  tcpip_get_hisIP(ip);
  send_arp(ip);
}