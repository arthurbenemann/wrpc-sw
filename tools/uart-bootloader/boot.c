/*
 * DSI Shield
 *
 * Copyright (C) 2013-2015 twl
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* main.c - main bootloader application */

#include <stdint.h>
#include <stdio.h>

#undef CONFIG_ERTM14_FLASH

#include "board.h"

#ifdef CONFIG_ERTM14_FLASH
    #include "dev/spi.h"
    #include "dev/gpio.h"
    #include "dev/spi_flash.h"
#endif

#ifndef CONFIG_USER_START
    #define CONFIG_USER_START 0x0
#endif

#include "dev/uart.h"

#define CMD_INIT 1
#define CMD_ERASE_SECTOR 2
#define CMD_WRITE_PAGE 3
#define CMD_WRITE_RAM 4
#define CMD_GO 5
#define CMD_GET_FLASH_ID 6


#define RSP_OK 1
#define RSP_HELLO 5
#define RSP_CRC_ERROR 2
#define RSP_VERIFY_ERROR 3
#define RSP_BAD_CRC 4

#define POLY 0x8408

#define RX_BUF_SIZE (256 + 16)

#define BOOT_TIMEOUT 500
#define UART_TIMEOUT 2000


uint8_t rxbuf[RX_BUF_SIZE];
int     boot_wait;
static uint32_t orig_reset_vector = 0x4;

typedef void (*voidfunc_t)();

struct simple_uart_device dev_uart;

#ifdef CONFIG_ERTM14_FLASH

#define BASE_AUXWB 0x48000


struct gpio_device gpio_aux;
struct spi_bus spi_flash;
struct spi_flash_device dev_flash;

static const struct gpio_pin pin_flash_cs_n = {  &gpio_aux, 55 };
static const struct gpio_pin pin_flash_miso = {  &gpio_aux, 53 };
static const struct gpio_pin pin_flash_mosi = {  &gpio_aux, 54 };
static const struct gpio_pin pin_flash_sck = {  &gpio_aux, 56 };

void  boot_flash_init()
{
    wb_gpio_create( &gpio_aux, BASE_AUXWB );
    bb_spi_create( &spi_flash,
        &pin_flash_cs_n,
        &pin_flash_mosi,
        &pin_flash_miso,
        &pin_flash_sck,
        10 );

    gen_gpio_set_dir( &pin_flash_mosi, 1 );
    gen_gpio_set_dir( &pin_flash_cs_n, 1 );
    gen_gpio_set_dir( &pin_flash_sck, 1 );

    spi_flash_create( &dev_flash, &spi_flash );
}

#endif

uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
    int i;

    crc = crc ^ (((uint16_t)data) << 8);

    for (i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
        {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

uint16_t
crc16(unsigned char *buf, int len)
{
    int i;
    uint16_t cksum;

    cksum = 0;

    for (i = 0; i < len; i++) {
        cksum = crc_xmodem_update(cksum, buf[i]);
    }
    return cksum;
}

int timeout_hit = 0;


uint8_t suart_read_blocking()
{
    uint32_t t_end = timer_get_tics() + UART_TIMEOUT;

    while (timer_get_tics() < t_end)
        if (suart_poll(&dev_uart))
            return suart_read_byte(&dev_uart);

    timeout_hit = 1;

    return 0;
}

void uart_readm_blocking(uint8_t *buf, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        buf[i] = suart_read_blocking();

        if (timeout_hit)
            return;
    }
}

void send_reply(uint8_t code, int length, uint8_t *data)
{
    uint8_t  buf[32];
    uint16_t crc, i;

    buf[0] = 0x55;
    buf[1] = 0xaa;
    buf[2] = code;
    buf[3] = (length >> 8) & 0xff;
    buf[4] = (length & 0xff);

    if(length > 0)
        memcpy(buf+5, data, length);

    crc = crc16(buf, length+5);

    buf[length+5] = (crc >> 8);
    buf[length+6] = (crc & 0xff);

    for (i = 0; i < length+7; i++)
        suart_write_byte(&dev_uart, buf[i]);
}

void on_cmd_init()
{
    boot_wait = 0;
    send_reply(RSP_OK, 0, NULL);
}

uint32_t unpack_be32(uint8_t *p)
{
    uint32_t rv = 0;

    rv |= p[3];
    rv |= ((uint32_t)p[2]) << 8;
    rv |= ((uint32_t)p[1]) << 16;
    rv |= ((uint32_t)p[0]) << 24;
    return rv;
}

#ifdef CONFIG_ERTM14_FLASH
void on_cmd_erase_sector(uint8_t *payload, int len)
{
    uint32_t base = unpack_be32(payload);

    spi_flash_erase_sector(&dev_flash, base);

    send_reply(RSP_OK, 0, NULL);
}

void on_cmd_write_page(uint8_t *payload, int len)
{
    uint32_t base = unpack_be32(payload);

    spi_flash_write(&dev_flash, base, payload + 4, len - 4);

    send_reply(RSP_OK, 0, NULL);
}

void on_cmd_get_flash_id(uint8_t *payload, int len)
{
    uint32_t id = spi_flash_read_id(&dev_flash);

    send_reply(RSP_OK, 4, &id);
}

#endif

void on_cmd_write_ram(uint8_t *payload, int len)
{
    int i;
    uint32_t base = unpack_be32(payload);

    for (i = 0; i < len - 4; i++)
    {
        if (base + i < 4)
        { // special case for the entry vector address
            switch(base + i)
            {
                case 1: orig_reset_vector = ((uint32_t)payload[i+4]) << 18; break;
                case 2: orig_reset_vector |= ((uint32_t)payload[i+4]) << 10; break;
                case 3: orig_reset_vector |= ((uint32_t)payload[i+4]) << 2; break;
                default:
                    break;
            }
        }
        else
        {
            *(uint8_t *)(base + i) = payload[i + 4];
        }
    }

    send_reply(RSP_OK, 0, NULL);
}

void on_cmd_go(uint8_t *payload, int len)
{
    uint32_t base = unpack_be32(payload);

    voidfunc_t f = (voidfunc_t)base;

    send_reply(RSP_OK, 0, NULL);

    f();
}

void boot_fsm()
{
    uint32_t t_exit = timer_get_tics() + BOOT_TIMEOUT;

    boot_wait = 1;

    send_reply(RSP_HELLO, 0, NULL);

    for (;;)
    {
        int pos = 0, i;
        uint16_t crc;

        if (boot_wait && (timer_get_tics() > t_exit))
            return;

        timeout_hit = 0;

        int c = suart_read_blocking();

        if ((c != 0x55) || timeout_hit)
        {
            continue;
        }
        rxbuf[pos++] = c;

        c = suart_read_blocking();

        if ((c != 0xaa) || timeout_hit)
            continue;

        rxbuf[pos++] = c;

        uint8_t command = suart_read_blocking();

        rxbuf[pos++] = command;
        rxbuf[pos++] = suart_read_blocking();
        rxbuf[pos++] = suart_read_blocking();

        uint16_t len = (uint16_t)rxbuf[3] << 8 | rxbuf[4];

        if (timeout_hit)
            continue;

        for (i = 0; i < len; i++)
            rxbuf[pos++] = suart_read_blocking();

        crc  = (uint16_t)suart_read_blocking() << 8;
        crc |= (uint16_t)suart_read_blocking();

        if (timeout_hit)
            continue;

        if (crc != crc16(rxbuf, len + 5))
        {
            send_reply(RSP_BAD_CRC, 0, NULL);
        }


        switch (command)
        {
        case CMD_INIT:
            on_cmd_init(rxbuf + 5, len);
            break;

        case CMD_ERASE_SECTOR:
        #ifdef CONFIG_ERTM14_FLASH
            on_cmd_erase_sector(rxbuf + 5, len);
        #endif
            break;

        case CMD_WRITE_PAGE:
        #ifdef CONFIG_ERTM14_FLASH
            on_cmd_write_page(rxbuf + 5, len);
        #endif
            break;

        case CMD_WRITE_RAM:
            on_cmd_write_ram(rxbuf + 5, len);
            break;

        case CMD_GO:
            on_cmd_go(rxbuf + 5, len);
            break;

#ifdef CONFIG_ERTM14_FLASH
        case CMD_GET_FLASH_ID:
            on_cmd_get_flash_id(rxbuf + 5, len);
            break;
#endif

        default:
            break;
        }
    }
}



void start_user()
{
    voidfunc_t f = (voidfunc_t)orig_reset_vector;

    f();
}

int main()
{
    suart_init( &dev_uart, BASE_UART, CONSOLE_UART_BAUDRATE );

    timer_init();
    #ifdef CONFIG_ERTM14_FLASH
        boot_flash_init();
    #endif
    boot_fsm();
    start_user();

    return 0;
}
