/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_BABYWR_H
#define __BOARD_BABYWR_H

#include "dev/gpio.h"
#include "dev/pca9554.h"
#include "lib/snmp.h"
/*
 * This is meant to be automatically included by the Makefile,
 * when wrpc-sw is build for wrc (node) -- as opposed to wrs (switch)
 */

/* BABYWR WB bus behind wr-cores Aux WB bus */
#define BASE_GPIO            (BASE_AUXWB + 0x000)
#define BASE_SIT5359_REFCLK  (BASE_AUXWB + 0x080)
#define BASE_SIT5359_DMTD    (BASE_AUXWB + 0x100)
#define BASE_SYSMON          (BASE_AUXWB + 0x400)

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

/* Number of Digital Frequency Control bits */
#define SIT5359_DFC_BITS 26

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* spll parameter that are board-specific */
// BABYWR has GENERIC_PHY_16BIT
#define BOARD_DIVIDE_DMTD_CLOCKS      0

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

// Main board LEDs and other IO on I2C GPIO
#define MAIN_BOARD_LED_0         WBGEN2_GEN_MASK(0, 1)
#define MAIN_BOARD_LED_1         WBGEN2_GEN_MASK(1, 1)
#define MAIN_BOARD_LED_2         WBGEN2_GEN_MASK(2, 1)
#define MAIN_BOARD_LED_3         WBGEN2_GEN_MASK(3, 1)
#define MAIN_BOARD_SEL_GROUP_0   WBGEN2_GEN_MASK(4, 1)
#define MAIN_BOARD_SEL_GROUP_1   WBGEN2_GEN_MASK(5, 1)
#define MAIN_BOARD_SEL_IRIG_B    WBGEN2_GEN_MASK(6, 1)
#define MAIN_BOARD_FAN_ENABLE    WBGEN2_GEN_MASK(7, 1)

/* Maximum number of files in the sdb filesystem.
   Need at least 4: ., sfp database, init script and calibration
   MAC address could also be written on sdbfs. */
#define SDBFS_REC 5

/* Maximum number of files in the sdb filesystem.
   Need at least 4: ., sfp database, init script and calibration
   MAC address could also be written on sdbfs. */
#define SDBFS_REC 5

/* Specific to this board (see board.c) */
/* I2C address of the storage eeprom */
#define FMC_EEPROM_ADR 0x50
#define UID_EEPROM_ADR 0x51
#define UID_OFFSET 0xfa

/* I2C address of the I2C multiplexer */
#define PCA9554_ADR 0x23

struct wr_sit5359_interface_device
{
    void *base_addr;
    uint8_t i2c_addr;
    struct gpio_pin pin_scl;
    struct gpio_pin pin_sda;
    struct gpio_device gpio_i2c;
    struct i2c_bus master;
    int pull_range, hsdiv;
    uint64_t rfreq;
};

struct babywr_board
{
    struct gpio_device gpio_aux;
    struct wr_sit5359_interface_device sit5359_refclk;
    struct wr_sit5359_interface_device sit5359_dmtd;
    struct pca9554_gpio_device gpio_main_board;
} typedef babywr_board;

void gpio_control_init(void);
int gpio_control_poll(void);
int  babywr_init(void);

void read_sitime (void);
void write_sitime (int dev, int val);
uint16_t temp_poll(void);
extern int phy_calibration_poll(void);
extern void phy_calibration_init(void);
extern void phy_calibration_disable(void);

/* macro for extended SNMP support on the SPEC7 board */
#define CONFIG_SNMP_BOARD_SPECIFIC

#if defined(CONFIG_SNMP) && defined(SNMP_SET)
// size of oid_wrpcBoardSpecificGroup
#define BOARD_OID_LENGTH 9
int set_select_group(uint8_t *buf, struct snmp_oid *obj);
int get_select_group(uint8_t *buf, struct snmp_oid *obj);
int set_irigb(uint8_t *buf, struct snmp_oid *obj);
int get_irigb(uint8_t *buf, struct snmp_oid *obj);
extern const struct snmp_oid oid_array_wrpcBoardSpecificGroup[];
extern const uint8_t oid_wrpcBoardSpecificGroup[BOARD_OID_LENGTH];
#endif


#endif /* __BOARD_BABYWR_H */
