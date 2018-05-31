#ifndef __TCPIP_CONFIG_H
#define __TCPIP_CONFIG_H

#include <stdint.h>

void tcpip_init(uint8_t mac_addr[]);
void tcpip_config(unsigned char *IP);

#endif
