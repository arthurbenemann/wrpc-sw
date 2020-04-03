/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __BOARD_ERTM14_H
#define __BOARD_ERTM14_H

#include "dev/gpio.h"
#include "dev/bb_spi.h"
#include "dev/ad951x.h"
#include "dev/ltc6950.h"
#include "dev/ad9910.h"
#include "dev/ad9520.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "ertm15_rf_distr.h"
#include "dds_sync_unit.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/iuart.h"
#include "rf_frame_transceiver.h"

#define BOARD_MAX_CONSOLE_DEVICES 2

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Center DMTD frequency (Hz) */
#define DMTD_CLOCK_FREQ_HZ 62500000

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
#  define BOARD_DIVIDE_DMTD_CLOCKS	0
#  define NS_PER_CLOCK 16

#define BOARD_MAX_CHAN_REF		1
#define BOARD_MAX_CHAN_AUX		2
#define BOARD_MAX_PTRACKERS		1

#ifdef CONFIG_IP
#define HAS_IP 1
#else
#define HAS_IP 0
#endif

#define ERTM14_MAX_CONFIGS 8

extern unsigned char *BASE_MINIC;
extern unsigned char *BASE_EP;

#define SDB_ADDRESS 0x50000

#define FMC_EEPROM_ADR 0x50


#define BASE_AUXWB                  0x48000
#define BASE_SOFTPLL                0x40200
#define BASE_PPS_GEN                0x40300
#define BASE_UART                   0x40500
#define BASE_SYSCON                 0x40400
#define BASE_EP                     0x40100
#define BASE_MINIC                  0x40000
#define BASE_ONEWIRE                0x40600
#define BASE_IUART_14               (BASE_AUXWB + 0x200)
#define BASE_ERTM14_DDS_SYNC_UNIT   (BASE_AUXWB + 0x300)
#define BASE_CLOCK_MONITOR          (BASE_AUXWB + 0x100)
#define BASE_ERTM14_10MHZ_ALIGN_UNIT       (BASE_AUXWB + 0x400)
#define BASE_ERTM14_RF_FRAME_TRANSCEIVER       (BASE_AUXWB + 0x500)
#define BASE_ERTM14_STREAMERS       (BASE_AUXWB + 0x600)


#define ERTM14_RF_OUT_MIN_ID 4
#define ERTM14_RF_OUT_MAX_ID 12

#define ERTM14_CLKAB_OUT_MIN_ID 0
#define ERTM14_CLKAB_OUT_MAX_ID 11

// clock monitor core channels (see ertm14_top.vhd for assignment to the clock monitor core)
#define ERTM14_CMON_CLK_SYS 0       /* system clock */
#define ERTM14_CMON_CLK_DMTD 1      /* DDMTD sampling clock */
#define ERTM14_CMON_CLK_PLL_FB 2    /* fixme: I don't remember, check in VHDL */
#define ERTM14_CMON_CLK_REF 3       /* WR REF clock (from the VCXO/OCXO) */
#define ERTM14_CMON_CLK_RX 4        /* RX clock (recovered by the WR PHY) */

#define ERTM14_CLKAB_OUT_FRONT_PANEL 11

// #define ERTM14_CALIBRATION_DEBUG 1

#define ERTM14_MODE_WITHOUT_ERTM15 (1 << 0)
#define ERTM14_MODE_OCXO_10MHZ (1 << 1)
#define ERTM14_MODE_OCXO_100MHZ (1 << 2)

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
    struct i2c_bus i2c_clka_distr;
    struct i2c_bus i2c_clkb_distr;

    struct ad951x_device ad9516_main;
    struct ad951x_device ad9516_ext;
    struct ltc6950_device ltc6950_pll;
    struct ad9910_device dds_ad9910_ref;
    struct ad9910_device dds_ad9910_lo;
    struct ad7888_device pwrmon_adc;
    struct ertm15_rf_distribution_device rf_distr;
    struct ad9520_device dev_clka_distr;
    struct ad9520_device dev_clkb_distr;
    struct i2c_bus i2c_mac_addr;
    struct m24aa025_device m24_mac_ids[2];
    struct dds_sync_unit_device dds_sync_dev;
    struct iuart_device iuart_14;
    struct wr_rf_frame_transceiver_device rf_xcvr;

    int mode;
};

struct ertm14_dds_config
{
    uint32_t ftw;
    uint8_t out_state[ERTM14_RF_OUT_MAX_ID + 1];
    int out_power[ERTM14_RF_OUT_MAX_ID + 1];
    int amp_power;
    int ampl_factor;
};

struct ertm14_board_config
{
    int valid;
    struct ertm14_dds_config ref;
    struct ertm14_dds_config lo;
    uint32_t clka_freq_hz[ERTM14_CLKAB_OUT_MAX_ID + 1];
    uint32_t clkb_freq_hz[ERTM14_CLKAB_OUT_MAX_ID + 1];
    uint32_t clka_enable_mask;
    uint32_t clkb_enable_mask;
};



extern struct ertm14_board board;

void ertm14_config_init(void);
struct ertm14_board_config *ertm14_get_config(int config_id);
int ertm14_apply_config(int config_id);
int ertm14_get_current_config_id(void);
int ertm14_is_config_ready(void);
int ertm14_get_clkab_divider( int freq );

#endif /* __BOARD_WRC_H */
