/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef ETHERBONE_CONFIG
#define ETHERBONE_CONFIG

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif

#if defined( __GNUC__)
#define PACKED __attribute__ ((packed))
#else
#error "Unsupported compiler?"
#endif


// /* These values are taken directly from the Etherbone specification */
// #define ERROR_STATUS_HIGH  0
// #define ERROR_STATUS_LOW   4
// #define SDB_ADDRESS_HIGH   8
// #define SDB_ADDRSES_LOW   12

// /* These are implementation specific */
// #define EB_MAC_HIGH16     16
// #define EB_MAC_LOW32      20
// #define EB_IPV4           24
// #define EB_PORT           28

PACKED struct EBCFG_WB {
  uint32_t ERROR_STATUS_HIGH;
  uint32_t ERROR_STATUS_LOW;
  uint32_t SDB_ADDRESS_HIGH;
  uint32_t SDB_ADDRSES_LOW;
  uint32_t EB_MAC_HIGH16;
  uint32_t EB_MAC_LOW32;
  uint32_t EB_IPV4;
  uint32_t EB_PORT;
};

#endif
