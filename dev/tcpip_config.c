#include <wrc.h>
#include <wrpc.h>
#include <string.h>

#include "endpoint.h"
#include "arp.h"
#include "hw/tcpip-config.h"
#include "tcpip_config.h"

extern uint8_t tcpip_status = TCPIP_NULL;

uint8_t tcpip_present()
{
  if ((*(uint16_t *)(BASE_TCPIP_CFG+TCPIP_STATUS_HIGH))> 0) 
  { 
    tcpip_status = TCPIP_PRS;
    return 1;
  } else
    return 0;
}

void tcpip_init(void)
{
  uint8_t tcpip_mac_addr[6];
  uint8_t tmp_ip_addr[4];
  
  if (!tcpip_present())
  {
    pp_printf("No TCPIP module is found!\n");
    return;
  }
  
  get_mac_addr(tcpip_mac_addr);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_HIGH16 + 2), (uint8_t *)tcpip_mac_addr, 2);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_MAC_LOW32), (uint8_t *)tcpip_mac_addr+2, 4);

  // default udp tx dst/src port
  tcpip_tx_dst_port(2000);
  tcpip_tx_src_port(2000);

  getIP(tmp_ip_addr);
  // tcpip module, default IP
  tcpip_ip_addr(tmp_ip_addr);
  // tcpip module, default gateway & tx ip addr
  tmp_ip_addr[3]=0x01;
  tcpip_gateway_addr(tmp_ip_addr);
  tcpip_set_hisIP(tmp_ip_addr);

  // tcpip module, default subnet mask
  tmp_ip_addr[0]=0xff;tmp_ip_addr[1]=0xff;tmp_ip_addr[2]=0xff;tmp_ip_addr[3]=0x00;
  tcpip_subnet_addr(tmp_ip_addr);
  
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
}

void tcpip_get_hisIP(uint8_t *ip)
{
  memcpy(ip, (uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_IP), 4);
}

void tcpip_set_hisMAC(uint8_t mac_addr[])
{
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_HIGH16 + 2), (uint8_t *)mac_addr, 2);
  memcpy((uint8_t *)(BASE_TCPIP_CFG + TCPIP_UDP_TX_DST_MAC_LOW32), (uint8_t *)mac_addr+2, 4);
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

  if (tcpip_status == TCPIP_OK)
    return 0;

  if (tcpip_status == TCPIP_ARP)
    arp_count++;

  if (arp_count<65530)
    return 0;
  
  tcpip_get_hisIP(ip);
  send_arp(ip);
  arp_count=0;

}

DEFINE_WRC_TASK(tcpip) = {
  .name = "tcpip",
  .enable = &tcpip_status,
  .init = tcpip_init,
  .job = tcpip_poll,
};

