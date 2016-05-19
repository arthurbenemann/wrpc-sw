/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef EXT_CONFIG
#define EXT_CONFIG

#define ERROR_STATUS_HIGH  0
#define ERROR_STATUS_LOW   4
#define SDB_ADDRESS_HIGH   8
#define SDB_ADDRSES_LOW   12

/* These are implementation specific */
#define EXT_MAC_HIGH16     16
#define EXT_MAC_LOW32      20
#define EXT_IP_ADDR        24
#define EXT_UDP_RX_PORT    28
#define EXT_UDP_TX_SRC_PORT 32
#define EXT_UDP_TX_DST_PORT 36
#define EXT_UDP_TX_DST_IP   40
#define EXT_UDP_TX_DST_MAC_HIGH16 44
#define EXT_UDP_TX_DST_MAC_LOW32 48
#define EXT_TCP_LOCAL_PORT 52

#endif
