/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

extern struct simple_uart_device console_uart;

void console_init(void);
int console_getc(void);

#endif

