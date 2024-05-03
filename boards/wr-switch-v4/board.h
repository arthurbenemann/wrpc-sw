/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __BOARD_WRS_10G_H
#define __BOARD_WRS_10G_H


#ifndef CONFIG_ARCH_RISCV
    #error wrsv4 board requires CONFIG_ARCH_RISCV
#endif

#define DEV_BASE	0x100000

#define SI57X_I2C_ADDR 0x55

#define CP_8V5816_I2C_ADDR      0x58
#define I2C_MUX_ADDR            0x70    
#define GPIO_EXP_I2C_ADDR       0x74
#define CP_8V5816_MUX_CH        2
#define GPIO_EXP_MUX_CH         2

#define GPIO_EXP_NUM_GPI        1
#define GPIO_EXP_NUM_GPO        6

#define GPIO_EXP_CLK_SW_RST_n   1<<7
#define GPIO_EXP_SI57X_OE1      1<<6
#define GPIO_EXP_SI53XX_RST     1<<5
#define GPIO_EXP_IN_SEL1        1<<4
#define GPIO_EXP_IN_SEL0        1<<3
#define GPIO_EXP_SI5341_INTR_n  1<<1
#define GPIO_EXP_SI5341_SYNC    1

#define AUX_I2C_PIN_SCL 1<<8
#define AUX_I2C_PIN_SDA 1<<9

#define TICS_PER_SECOND 100000

#define CPU_CLOCK             62500000
#define REF_CLOCK_FREQ_HZ     62500000
#define NS_PER_CLOCK          16
#define REF_CLOCK_PERIOD_PS   16000


#define BOARD_CONSOLE_DEVICES 1
#define BOARD_USE_EVENTS 0


#define UART_BAUDRATE 115200

/* RT CPU Memory layout */
#define BASE_UART (DEV_BASE + 0x10000)
#define BASE_SOFTPLL (DEV_BASE + 0x10100)
#define BASE_SPI (DEV_BASE + 0x10200)
#define BASE_GPIO (DEV_BASE + 0x10300)
#define BASE_TIMER (DEV_BASE + 0x10400)
#define BASE_PPS_GEN (DEV_BASE + 0x10500)
#define BASE_SPI_LJD_BOARD (DEV_BASE + 0x10700)
#define BASE_SI57X_INTERFACE (DEV_BASE + 0x10800)
#define BASE_CP_8V5816 (DEV_BASE + 0x10300)      


/* spll parameter that are board-specific */
#define BOARD_DIVIDE_DMTD_CLOCKS	0
#define BOARD_MAX_CHAN_REF		18
#define BOARD_MAX_CHAN_AUX		1
#define BOARD_MAX_PTRACKERS		18

#define CONSOLE_UART_BAUDRATE 115200
#define BOARD_MAX_CONSOLE_DEVICES 1

// fixme:
//#define CONFIG_WR_SWITCH_V4

#endif
