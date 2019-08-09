/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_ERTM14_H
#define __BOARD_ERTM14_H

/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for ERTM14 (MTCA.4 WR node) -- as opposed to wrs (switch)
 * or vanilla WR core
 */

#include "dev/gpio.h"
#include "dev/spi.h"
#include "dev/ad951x.h"
#include "dev/ltc6950.h"
#include "dev/ad9910.h"
#include "dev/ad9520.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/ertm15_rf_distr.h"
#include "dev/ertm14_dds_sync.h"
#include "dev/spi_flash.h"
#include "dev/i2c.h"
#include <hw/memlayout.h>

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define CONSOLE_UART_BAUDRATE 921600ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

int board_init(void);
int board_update(void);

/* spll parameter that are board-specific */
#ifdef CONFIG_WR_NODE_PCS16
#  define BOARD_DIVIDE_DMTD_CLOCKS	0
#else
#  define BOARD_DIVIDE_DMTD_CLOCKS	1
#endif
#define BOARD_MAX_CHAN_REF		1
#define BOARD_MAX_CHAN_AUX		2
#define BOARD_MAX_PTRACKERS		1

#ifdef CONFIG_IP
#define HAS_IP 1
#else
#define HAS_IP 0
#endif


#define BASE_AUXWB 0x48000
#define BASE_CLOCK_MONITOR  0x48100
#define BASE_SOFTPLL 0x40200
#define BASE_PPS_GEN 0x40300
#define BASE_ERTM14_DDS_SYNC_UNIT  0x48300

struct ertm14_board 
{
    struct gpio_device gpio_aux;
    struct wb_clock_monitor_device ertm14_cmon;

    struct spi_bus spi_pll_main;
    struct spi_bus spi_pll_ext;
    struct spi_bus spi_ltc6950;
    struct spi_bus spi_ad9910_ref;
    struct spi_bus spi_ad9910_lo;
    struct spi_bus spi_ocxo_dac;
    struct spi_bus spi_ad7888;
    struct spi_bus spi_flash;
    struct i2c_bus i2c_clka_distr;
    struct i2c_bus i2c_clkb_distr;

    struct ad951x_device ad9516_main;
    struct ad951x_device ad9516_ext;
    struct ltc6950_device ltc6950_pll;
    struct ad9910_device dds_ad9910_ref;
    struct ad9910_device dds_ad9910_lo;
    struct ad7888_device pwrmon_adc;
    struct ertm15_rf_distribution_device rf_distr;
    struct spi_flash_device dev_flash;
    struct ad9520_device dev_clka_distr;
    struct ad9520_device dev_clkb_distr;
    struct i2c_bus i2c_mac_addr;
    struct m24aa025_device m24_mac_ids[2];
    struct dds_sync_unit_device dds_sync_dev;
};

extern struct ertm14_board board;

#endif /* __BOARD_WRC_H */
