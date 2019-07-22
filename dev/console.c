/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdio.h>

#include "board.h"
#include "dev/uart.h"
#include "dev/console.h"

struct simple_uart_device console_uart;

int puts(const char *s)
{
	return suart_write_string(&console_uart, s);
}

int console_getc()
{
    return suart_read_byte(&console_uart);
}

void console_init()
{
    suart_init( &console_uart, BASE_UART, CONSOLE_UART_BAUDRATE );
	console_uart.crlf_mode = 1;
}