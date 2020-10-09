/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#ifndef __WRC_NETIF_H
#define __WRC_NETIF_H

struct wrc_endpoint_dev;

#define NETIF_LINK_DOWN 0
#define NETIF_LINK_WENT_UP 1
#define NETIF_LINK_WENT_DOWN 2
#define NETIF_LINK_UP 3

struct wrc_netif_device
{
    const char* name;
    const char* desc;
    struct wr_endpoint_device* ep;
    int link_state;
    int rx_packets;
    int tx_packets;
};

struct wr_endpoint_device* netif_get_default_endpoint(void);
int netif_register_device( const char *name, const char* desc, struct wr_endpoint_device* ep );
int netif_get_device_count(void);
struct wrc_netif_device* netif_get_device(int idx);
int netif_init(void);

#endif
