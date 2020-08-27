/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#define CONSOLE_FLAGS_MODE_BINARY (1<<0)
#define CONSOLE_FLAGS_MODE_TTY    (1<<1)
#define CONSOLE_FLAGS_INSERT_CRLF (1<<2)


struct console_device {
    int (*get_char)( struct console_device *dev );
    int (*put_string)( struct console_device *dev, const char *str );
    void *priv;
    int flags;
};

extern struct console_device *console;


void console_set_device( struct console_device *dev );
void console_uart_write_bytes( uint8_t *buf, int count );
void console_uart_set_crlf_mode(int on);
void console_init(void);
int console_getc(void);

void console_force_mode( struct console_device *dev, int mode );
int console_get_mode( struct console_device *dev );

int console_binary_send( struct console_device *dev, void *buff, int size );
int console_binary_recv( struct console_device *dev, void *buff, int size );

#ifdef CONFIG_IPMI_CONSOLE
int console_ipmi_process_request(struct console_device* dev,  uint8_t *req, int size, uint8_t *rsp, int rsp_size );
#endif

#endif

