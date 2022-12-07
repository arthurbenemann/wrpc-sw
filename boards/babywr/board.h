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

#ifdef CONFIG_ARCH_RISCV
#define DEV_BASE	0x100000
#elif defined CONFIG_ARCH_LM32
#define DEV_BASE	0x40000
#else
#error (Wrong Arch!)
#endif

/* Fixed base addresses */
#define BASE_MINIC            (DEV_BASE + 0x000)
#define BASE_EP               (DEV_BASE + 0x100)
#define BASE_SOFTPLL          (DEV_BASE + 0x200)
#define BASE_PPS_GEN          (DEV_BASE + 0x300)
#define BASE_SYSCON           (DEV_BASE + 0x400)
#define BASE_UART             (DEV_BASE + 0x500)
#define BASE_ONEWIRE          (DEV_BASE + 0x600)
#define BASE_WDIAGS_PRIV      (DEV_BASE + 0x900)
#define BASE_AUXWB            (DEV_BASE + 0x8000)

/* BABYWR WB bus behind wr-cores Aux WB bus */
#define BASE_GPIO            (BASE_AUXWB + 0x000)
#define BASE_SIT5359_REFCLK  (BASE_AUXWB + 0x080)
#define BASE_SIT5359_DMTD    (BASE_AUXWB + 0x100)

/* Board-specific parameters */
#define TICS_PER_SECOND 1000

/* WR Core system/CPU clock frequency in Hz */
#define CPU_CLOCK 62500000ULL

/* WR Reference clock period (picoseconds) and frequency (Hz) */
/* GENERIC_PHY_16BIT */
#define NS_PER_CLOCK 16
#define REF_CLOCK_PERIOD_PS 16000
#define REF_CLOCK_FREQ_HZ 62500000

/* Baud rate of the builtin UART (does not apply to the VUART) */
#define UART_BAUDRATE 115200ULL

/* Maximum number of simultaneously created sockets */
#define NET_MAX_SOCKETS 12

/* Socket buffer size, determines the max. RX packet size */
#define NET_MAX_SKBUF_SIZE 512

/* Number of auxillary clock channels - usually equal to the number of FMCs */
#define NUM_AUX_CLOCKS 1

/* spll parameter that are board-specific */
// BABYWR has GENERIC_PHY_16BIT
#  define BOARD_DIVIDE_DMTD_CLOCKS    0

/* BABYWR uses CRYSTEC_CVPD992 for Helper and Main VCXO */
#define MAIN_CRYSTEK_CVPD922   1
#define HELPER_CRYSTEK_CVPD922 1

#define BOARD_MAX_CHAN_REF            1
#define BOARD_MAX_CHAN_AUX            2
#define BOARD_MAX_PTRACKERS           1

#define CONFIG_DISALLOW_LONG_DIVISION

#define BOARD_USE_EVENTS 1

#define BOARD_MAX_CONSOLE_DEVICES (1 + HAS_NETCONSOLE + HAS_PUTS_SYSLOG)

#define CONSOLE_UART_BAUDRATE 115200

#define SDB_ADDRESS 0x30000

#define FMC_EEPROM_ADR 0x50
#define UID_EEPROM_ADR 0x51
#define UID_OFFSET 0xfa

#define SDBFS_REC 5

#ifdef CONFIG_IP
#define HAS_IP 1
#else
#define HAS_IP 0
#endif

#ifdef CONFIG_ABSCAL
#define HAS_ABSCAL 1
#else
#define HAS_ABSCAL 0
#endif

int  babywr_init(void);

/*
struct babywr_board
{
    struct gpio_device gpio_aux;
};
*/

void sdb_find_devices(void);
void sdb_print_devices(void);

#endif /* __BOARD_BABYWR_H */
