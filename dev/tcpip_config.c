#include <string.h>

#include "endpoint.h"
#include "hw/tcpip-config.h"
#include "tcpip_config.h"

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
}

void tcpip_config(unsigned char *IP)
{
  unsigned int *tcpip_tmp;
	uint32_t tmp;


  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_IP_ADDR);
	tmp = (*IP << 24) | (*(IP+1) << 16) | (*(IP+2) << 8) | (*(IP+3));
  *tcpip_tmp = tmp;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_GATEWAY);
	tmp = (tmp & 0xFFFFFF00) + 1;
  *tcpip_tmp = tmp;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_SUBNET_MASK);
	tmp = 0xFFFFFF00;
  *tcpip_tmp = tmp;

  tcpip_tmp = (unsigned int *)(BASE_TCPIP_CFG + TCPIP_TCP_LOCAL_PORT);
  tmp = 8000;
  *tcpip_tmp = tmp;
}
