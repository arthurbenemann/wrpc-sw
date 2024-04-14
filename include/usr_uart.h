/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __USR_UART_H
#define __USR_UART_H

void usr_uart_init_hw(void);
void usr_uart_write_byte(int b);
int usr_uart_write_string(const char *s);
int usr_uart_read_byte(void);

#endif
