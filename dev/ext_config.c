#include <string.h>

#include "endpoint.h"
#include "hw/ext-config.h"
#include "ext_config.h"

void ext_config(unsigned char *IP,unsigned char *MAC)
{
  unsigned int *ext_tmp;
	unsigned int tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_MAC_HIGH16);
  tmp = (*MAC << 8) | (*(MAC+1));
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_MAC_LOW32);
  tmp = (*(MAC+2) << 24) | (*(MAC+3) << 16) | (*(MAC+4) << 8) | (*(MAC+5));
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_IP_ADDR);
	tmp = (*IP << 24) | (*(IP+1) << 16) | (*(IP+2) << 8) | (*(IP+3));
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
	tmp = 0x9890;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_UDP_TX_DST_MAC_LOW32);
  tmp = 0x96aa9e04;
  *ext_tmp = tmp;

  ext_tmp = (unsigned int *)(BASE_EXT_CFG + EXT_TCP_LOCAL_PORT);
  tmp = 8000;
  *ext_tmp = tmp;
}
