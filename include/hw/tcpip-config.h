/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef TCPIP_CONFIG
#define TCPIP_CONFIG

#define TCPIP_STATUS_HIGH  0
#define TCPIP_STATUS_LOW   4
#define SDB_ADDRESS_HIGH   8
#define SDB_ADDRSES_LOW   12

/* These are implementation specific */
#define TCPIP_MAC_HIGH16     16
#define TCPIP_MAC_LOW32      20
#define TCPIP_IP_ADDR        24
#define TCPIP_GATEWAY        28
#define TCPIP_SUBNET_MASK    32 
#define TCPIP_UDP_RX_PORT    36 
#define TCPIP_UDP_TX_DST_PORT 40 
#define TCPIP_UDP_TX_SRC_PORT 44 
#define TCPIP_UDP_TX_DST_IP   48
#define TCPIP_UDP_TX_DST_MAC_HIGH16 52 
#define TCPIP_UDP_TX_DST_MAC_LOW32 56 
#define TCPIP_TCP_LOCAL_PORT 60 

#endif
