#include <string.h>

#include "endpoint.h"
#include "hw/ext-config.h"
#include "ext_config.h"

void ext_init(uint8_t mac_addr[])
{
  volatile unsigned int *ext_tmp;
  uint32_t tmp=0;
  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_MAC_HIGH16);
  tmp = ((uint32_t) mac_addr[0] << 8)
      | ((uint32_t) mac_addr[1]);
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_MAC_LOW32);
  tmp = ((uint32_t) mac_addr[2] << 24)
      | ((uint32_t) mac_addr[3] << 16)
      | ((uint32_t) mac_addr[4] << 8)
      | ((uint32_t) mac_addr[5]);
  *ext_tmp = tmp;
}

void ext_config(unsigned char *IP)
{
  unsigned int *ext_tmp;
	uint32_t tmp;


  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_IP_ADDR);
	tmp = (*IP << 24) | (*(IP+1) << 16) | (*(IP+2) << 8) | (*(IP+3));
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_GATEWAY);
	tmp = (tmp & 0xFFFFFF00) + 1;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_SUBNET_MASK);
	tmp = 0xFFFFFF00;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_RX_PORT);
	tmp = 60000;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_SRC_PORT);
	tmp = 60001;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_DST_PORT);
	tmp = 60002;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_DST_IP);
	tmp = 0xC0A80001;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_DST_MAC_HIGH16);
	//tmp = 0x9890;
    tmp = 0x8cae;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_DST_MAC_LOW32);
    //tmp = 0x96aa9e04;
    tmp = 0x4b002f40;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_TCP_LOCAL_PORT);
  tmp = 8000;
  *ext_tmp = tmp;
}
