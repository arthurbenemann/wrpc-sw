/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#include <wrc.h>
#include "board.h"
#include "dev/etherbone.h"

#include "hw/etherbone-config.h"

#define ebcfg_write(reg, val) \
	*(volatile uint32_t *) (BASE_ETHERBONE_CFG + (offsetof(struct EBCFG_WB, reg))) = (val)

#define ebcfg_read(reg) \
	*(volatile uint32_t *) (BASE_ETHERBONE_CFG + (offsetof(struct EBCFG_WB, reg)))


void eb_setIP(unsigned char *ip) {
	uint32_t ebip;
	ebip = (uint32_t) (ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3]); 
	ebcfg_write(EB_IPV4, ebip);
}

void eb_readIP(unsigned char * ip) {
	uint32_t eb_ip;	
	eb_ip = ebcfg_read(EB_IPV4);
	ip[0] = (eb_ip>>24)&(0xff);
	ip[1] = (eb_ip>>16)&(0xff);
	ip[2] = (eb_ip>>8)&(0xff);
	ip[3] = (eb_ip>>0)&(0xff);
}


void eb_readMAC(uint8_t * mac) {
	uint32_t ebmac_low, ebmac_high;	
	ebmac_high = ebcfg_read(EB_MAC_HIGH16) & 0xffff;
	ebmac_low  = ebcfg_read(EB_MAC_LOW32);
	if(mac){
		mac[0] = (ebmac_high & 0x0000ff00) >> 8; 
		mac[1] = (ebmac_high & 0x000000ff); 
		mac[2] = (ebmac_low & 0xff000000) >> 24;
		mac[3] = (ebmac_low & 0x00ff0000) >> 16; 
		mac[4] = (ebmac_low & 0x0000ff00) >> 8; 
		mac[5] = (ebmac_low & 0x000000ff); 
	}
}

void eb_readPort(uint32_t * port) {
	uint32_t ebport;	
	ebport  = ebcfg_read(EB_PORT);
	if(port){
		*port = ebport;
	}
}

// void eb_setMAC()
// void eb_setPort()
