#include <wrc.h>
#include <wrpc.h>
#include <string.h>

#include "endpoint.h"
#include "arp.h"
#include "lib/ipv4.h"
#include "hw/tcpip-config.h"
#include "tcpip_config.h"

extern uint8_t tcpip_status = TCPIP_NULL;

uint8_t tcpip_present()
{
  if ((*(uint16_t *)(BASE_TCPIP_CFG+TCPIP_STATUS_HIGH))> 0) 
  { 
    return 1;
  } else
    return 0;
}

void tcpip_init(void)
{
  uint8_t tcpip_mac_addr[6];
  
  if (!tcpip_present())
  {
    pp_printf("No TCPIP module is found!\n");
    return;
  }
  
  get_mac_addr(tcpip_mac_addr);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_HIGH16 + 2), (uint8_t *)tcpip_mac_addr, 2);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_LOW32), (uint8_t *)tcpip_mac_addr+2, 4);

  // default udp tx dst/src port
  tcpip_tx_src_port(2000);
  tcpip_tx_dst_port(2000);
  tcpip_rx_tcp_port(8000);

  tcpip_status = TCPIP_PRS;
 
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
  volatile uint16_t *rdp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_UDP_RX_PORT + 2);  
  *rdp = (uint16_t)port; 
  // enable setting rx dst port
  memset((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_RX_PORT), 0xffff, 2);
}

void tcpip_tx_src_port(uint16_t port)
{
  volatile uint16_t *tsp =
      (uint16_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_SRC_PORT + 2);  
  *tsp = (uint16_t)port;
}

void tcpip_tx_dst_port(uint16_t port)
{
  volatile uint16_t *tdp =
      (uint16_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_PORT + 2);  
  *tdp = (uint16_t)port;
}

void tcpip_set_hisIP(uint8_t *ip)
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_IP), ip, 4);
  // hisMAC is not ready
  memset((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_HIGH16), 0x0000, 2);
  tcpip_status = TCPIP_ARP;
}

void tcpip_get_hisIP(uint8_t *ip)
{
  memcpy(ip, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_IP), 4);
}

void tcpip_set_hisMAC(uint8_t mac_addr[])
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_LOW32), (uint8_t *)mac_addr+2, 4);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_HIGH16 + 2), (uint8_t *)mac_addr, 2);
  // hisMAC is ready
  memset((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_HIGH16), 0xffff, 2);
}

void tcpip_get_hisMAC(uint8_t mac_addr[])
{
  memcpy(mac_addr, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_HIGH16+2), 2);
  memcpy(mac_addr+2, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_LOW32), 4);
}

void tcpip_rx_tcp_port(uint16_t port)
{
  volatile unsigned int *rtp =
      (unsigned int *)(BASE_TCPIP_CFG + TCPIP_TCP_LOCAL_PORT);  
  *rtp = (uint32_t)port;
}

uint8_t tcpip_poll()
{
  uint8_t * ip;
  static uint16_t arp_count = 0;

  if (tcpip_status == TCPIP_OK || ip_status == IP_TRAINING)
    return 0;
  
  if (tcpip_status == TCPIP_PRS)
  {
    getIP(ip);
    // tcpip module, default IP

    tcpip_ip_addr(ip);
    // tcpip module, default gateway & tx ip addr
    *(ip+3)=0x01;
    tcpip_gateway_addr(ip);
    tcpip_set_hisIP(ip);

    // tcpip module, default subnet mask
    
    memset(ip, 0xffffff00, 4);
    tcpip_subnet_addr(ip);
  }

  if (tcpip_status == TCPIP_ARP)
    arp_count++;

  if (arp_count<30000)
    return 0;

  tcpip_get_hisIP(ip);
  send_arp(ip);
  arp_count=0;
  return 1;

}

DEFINE_WRC_TASK(tcpip) = {
  .name = "tcpip",
  .enable = &tcpip_status,
  .init = tcpip_init,
  .job = tcpip_poll,
};

