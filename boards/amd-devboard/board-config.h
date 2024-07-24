/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2023-2024 Missing Link Electronics (www.missinglinkelectronics.com)
 * Author: Frederik Pfautsch <frederik.pfautsch@missinglinkelectronics.com>
 *         Oskar Szakinnis <oskar.szakinnis@missinglinkelectronics.com>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */
#ifndef __BOARD_CONFIG_AMD_DEVBOARD_H
#define __BOARD_CONFIG_AMD_DEVBOARD_H
/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for wrc (node) -- as opposed to wrs (switch)
 */

/* Support etherbone: define the address of the etherbone core. */
#define BASE_ETHERBONE_CFG BASE_AUXWB

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#define NS_PER_CLOCK 16
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* spll parameter that are board-specific */
/* For timing closure, DMTD clock is divided by 2 when the RX clock
  is 125Mhz. This is controlled by the g_divide_input_by_2 generic
  in wr_core.vhd */
#define BOARD_DIVIDE_DMTD_CLOCKS 0

/* Number of reference channels (RX clocks) */
#define BOARD_MAX_CHAN_REF 1
/* Number of external pll that can be disciplined */
#define BOARD_MAX_CHAN_AUX 2
/* Should be the same as reference channels */
#define BOARD_MAX_PTRACKERS 1

/* Events are not used on this platform */
#define BOARD_USE_EVENTS 0

/* Use one uart at 115200 baud. Some boards may add extra uart. */
#define BOARD_CONSOLE_DEVICES 1
#define CONSOLE_UART_BAUDRATE 115200

/* i2c mux parameters */
#define ZCU102_I2C_MUX0_ADR 0x74
#define ZCU102_I2C_MUX0_CH_BIT_EEPROM (1 << 0)
#define ZCU102_I2C_MUX0_CH_BIT_SI5341 (1 << 1)
#define ZCU102_I2C_MUX0_CH_BIT_SI570 (1 << 2)

#define ZCU102_I2C_MUX1_ADR 0x75
#define ZCU102_I2C_MUX1_CH_BIT_SFP0 (1 << 7)
#define ZCU102_I2C_MUX1_CH_BIT_SFP1 (1 << 6)
#define ZCU102_I2C_MUX1_CH_BIT_SFP2 (1 << 5)
#define ZCU102_I2C_MUX1_CH_BIT_SFP3 (1 << 4)

/* i2c eeprom properties */
#define EEPROM_M24C08_ADR 0x54
#define EEPROM_M24C08_BYTE_OFFSET -2
#define EEPROM_HDMI_EDID_ADR 0x50
#define EEPROM_HDMI_EDID_BYTE_OFFSET 2

/* Maximum number of files in the sdb filesystem.
   Need at least 4: ., sfp database, init script and calibration
   MAC address could also be written on sdbfs. */
#define SDBFS_REC 5

#endif /* __BOARD_CONFIG_AMD_DEVBOARD_H */
