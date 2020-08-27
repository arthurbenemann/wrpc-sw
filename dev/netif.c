/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <stdio.h>
#include <wrc.h>

#include <dev/endpoint.h>
#include <dev/netif.h>

#include "board.h"

#ifndef WRC_NETIF_MAX_DEVICES
    #define WRC_NETIF_MAX_DEVICES 2
#endif

static int netif_n_count = 0;
static struct wrc_netif_device netif_devs[WRC_NETIF_MAX_DEVICES];

struct wrc_endpoint_dev* netif_get_default_endpoint()
{
    if( netif_n_count == 0 )
        return NULL;
    
    return netif_devs[0].ep;
}

int netif_register_device( const char *name, const char* desc, struct wr_endpoint_device* ep )
{
    if( netif_n_count >= WRC_NETIF_MAX_DEVICES )
        return -1;

    struct wrc_netif_device *ndev = &netif_devs[ netif_n_count ];
    netif_n_count++;

    ndev->name = name;
    ndev->ep = ep;
    ndev->desc= desc;
    ndev->rx_packets = 0;
    ndev->tx_packets = 0;
    ndev->link_state = NETIF_LINK_DOWN;

    dev_dbg("Registered network interface %s @ %p\n", ndev->name, ndev->ep->base );

    return 0;
}

int netif_get_device_count()
{
    return netif_n_count;
}

static int netif_update_task()
{
    int i;
    for( i = 0; i < netif_n_count; i++ )
    {
        struct wrc_netif_device *ndev = &netif_devs[ i ];

        int up = ep_link_up( ndev->ep, NULL );
        //pp_printf("%s link %d\n", ndev->name, up );
        switch(ndev->link_state)
        {
            case NETIF_LINK_DOWN:
                if( up )
                    ndev->link_state = NETIF_LINK_WENT_UP;
                break;
            case NETIF_LINK_UP:
                if( !up )
                    ndev->link_state = NETIF_LINK_WENT_DOWN;
                break;
            case NETIF_LINK_WENT_UP:
                if( up )
                    ndev->link_state = NETIF_LINK_UP;
                else
                    ndev->link_state = NETIF_LINK_WENT_DOWN;
                break;
            case NETIF_LINK_WENT_DOWN:
                if( up )
                    ndev->link_state = NETIF_LINK_WENT_UP;
                else
                    ndev->link_state = NETIF_LINK_DOWN;
                break;
            default:
                break;
        }
    }

    return 0;
}


struct wrc_netif_device* netif_get_device(int idx)
{
    return &netif_devs[idx];
}


int netif_init()
{
    wrc_task_create("netif", NULL, netif_update_task);
    return 0;
}