/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __EVENT_H
#define __EVENT_H

int event_handler_register( int mask, int enable, void (*func)(int) );
int event_handler_enable( int id, int enable );
int event_post( int event );
int events_init(void);
int events_dispatch(void);

#endif