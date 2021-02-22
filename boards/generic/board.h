/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_WRC_H
#define __BOARD_WRC_H
/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for wrc (node) -- as opposed to wrs (switch)
 */

/* Fixed base addresses */
#define BASE_MINIC	0x80000000
#define BASE_EP		0x80000100
#define BASE_SOFTPLL 	0x80000200
#define BASE_PPS_GEN 	0x80000300
#define BASE_SYSCON	0x80000400
#define BASE_UART	0x80000500
#define BASE_ONEWIRE	0x80000600
#define BASE_ETHERBONE_CFG	0x80008000

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#ifdef CONFIG_TARGET_GENERIC_PHY_16BIT
#  define NS_PER_CLOCK 16
#  define REF_CLOCK_PERIOD_PS 16000
#  define REF_CLOCK_FREQ_HZ 62500000
#else
#  define NS_PER_CLOCK 8
#  define REF_CLOCK_PERIOD_PS 8000
#  define REF_CLOCK_FREQ_HZ 125000000
#endif

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define UART_BAUDRATE 115200ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

/* spll parameter that are board-specific */
#ifdef CONFIG_TARGET_GENERIC_PHY_16BIT
#  define BOARD_DIVIDE_DMTD_CLOCKS	0
#else
#  define BOARD_DIVIDE_DMTD_CLOCKS	1
#endif
#define BOARD_MAX_CHAN_REF		1
#define BOARD_MAX_CHAN_AUX		2
#define BOARD_MAX_PTRACKERS		1

#undef CONFIG_DISALLOW_LONG_DIVISION

#define BOARD_MAX_CONSOLE_DEVICES 1

#define CONSOLE_UART_BAUDRATE 115200

#define SDB_ADDRESS 0x30000

#define FMC_EEPROM_ADR 0x50

#define SDBFS_REC 5

#define EEPROM_STORAGE 0

void sdb_find_devices(void);
void sdb_print_devices(void);

#endif /* __BOARD_WRC_H */
