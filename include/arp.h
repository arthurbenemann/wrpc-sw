/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef ARP_H
#define ARP_H

#include <inttypes.h>
#include "ptpd_netif.h" /* for sockaddr in prototype */

#define ARP_HTYPE    0
#define ARP_PTYPE    (ARP_HTYPE+2)
#define ARP_HLEN    (ARP_PTYPE+2)
#define ARP_PLEN    (ARP_HLEN+1)
#define ARP_OPER    (ARP_PLEN+1)
#define ARP_SHA        (ARP_OPER+2)
#define ARP_SPA        (ARP_SHA+6)
#define ARP_THA        (ARP_SPA+4)
#define ARP_TPA        (ARP_THA+6)
#define ARP_END        (ARP_TPA+4)

int send_arp(uint8_t * hisIP, int port);
#endif
