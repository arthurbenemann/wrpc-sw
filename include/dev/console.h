/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

struct console_device {
    int (*get_char)( struct console_device *dev );
    int (*put_string)( struct console_device *dev, const char *str );
    void *priv;
};

extern struct console_device *console;

void console_set_device( struct console_device *dev );

void console_init(void);
int console_getc(void);

#ifdef CONFIG_ERTM14
int console_ipmi_process_request(struct console_device* dev,  uint8_t *req, int size, uint8_t *rsp, int rsp_size );
#endif

#endif

