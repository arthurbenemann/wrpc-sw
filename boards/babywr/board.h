/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_BABYWR_H
#define __BOARD_BABYWR_H

#include "dev/gpio.h"

/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for wrc (node) -- as opposed to wrs (switch)
 */

/* BABYWR WB bus behind wr-cores Aux WB bus */
#define BASE_GPIO            (BASE_AUXWB + 0x000)
#define BASE_SIT5359_REFCLK  (BASE_AUXWB + 0x080)
#define BASE_SIT5359_DMTD    (BASE_AUXWB + 0x100)

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz (clk_sys) */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
/* GENERIC_PHY_16BIT */
#define NS_PER_CLOCK 16
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Accomodate 20 bit MAX5719A */
#define BOARD_SPLL_DAC_BITS 20
#define BOARD_SPLL_DIV_BITS 4

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* spll parameter that are board-specific */
// BABYWR has GENERIC_PHY_16BIT
#  define BOARD_DIVIDE_DMTD_CLOCKS    0

/* BABYWR uses CRYSTEC_CVPD992 for Helper and Main VCXO */
#define MAIN_CRYSTEK_CVPD922   1
#define HELPER_CRYSTEK_CVPD922 1

/* Number of reference channels (RX clocks) */
#define BOARD_MAX_CHAN_REF            1
/* Number of external pll that can be disciplined */
#define BOARD_MAX_CHAN_AUX            2
/* Should be the same as reference channels */
#define BOARD_MAX_PTRACKERS           1

/* Events are not used on this platform */
#define BOARD_USE_EVENTS 0

/* Use one uart at 115200 baud. May add extra uart. */
#define BOARD_CONSOLE_DEVICES 1
#define CONSOLE_UART_BAUDRATE 115200

/* Maximum number of files in the sdb filesystem.
   Need at least 4: ., sfp database, init script and calibration
   MAC address could also be written on sdbfs. */
#define SDBFS_REC 5

/* Specific to this board (see board.c) */
/* I2C address of the storage eeprom */
#define FMC_EEPROM_ADR 0x50
#define UID_EEPROM_ADR 0x51
#define UID_OFFSET 0xfa

int  babywr_init(void);

void read_sitime (void);
void write_sitime (int dev, int val);

extern int phy_calibration_poll(void);
extern void phy_calibration_init(void);
extern int phy_calibration_done(void);
extern void phy_calibration_disable(void);

#endif /* __BOARD_BABYWR_H */
