/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __EB_CFG_H
#define __EB_CFG_H

#include <stdint.h>

void eb_setIP(unsigned char *ip);
void eb_readIP(unsigned char *ip);
void eb_readMAC(unsigned char *mac);
void eb_readPort(uint32_t * port);

#endif
