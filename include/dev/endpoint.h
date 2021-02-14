/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __ENDPOINT_H
#define __ENDPOINT_H

#include <stdint.h>

typedef enum {
	AND = 0,
	NAND = 4,
	OR = 1,
	NOR = 5,
	XOR = 2,
	XNOR = 6,
	MOV = 3,
	NOT = 7
} pfilter_op_t;

void ep_init(uint8_t mac_addr[],int port);
void get_mac_addr(uint8_t dev_addr[],int port);
void set_mac_addr(uint8_t dev_addr[],int port);
int ep_enable(int enabled, int autoneg,int port);
int ep_link_up(uint16_t * lpa,int port);
int ep_get_bitslide(int port);
int ep_get_deltas(uint32_t * delta_tx, uint32_t * delta_rx,int port);
int ep_get_psval(int32_t * psval,int port);
int ep_cal_pattern_enable(int port);
int ep_cal_pattern_disable(int port);
int ep_timestamper_cal_pulse(int port);
int ep_sfp_enable(int ena,int port);
int ep_reset(int port);

void pfilter_init_default(int port);

#endif
