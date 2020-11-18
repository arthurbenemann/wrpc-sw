#ifndef __ERTM14_UART_LINK_H
#define __ERTM14_UART_LINK_H

#include <stdint.h>

#define ERTM14_MAX_UART_LINK_PAYLOAD 512

struct simple_uart_device;

struct uart_packet
{
    uint8_t ptype;
    uint16_t length;
    uint8_t payload[ ERTM14_MAX_UART_LINK_PAYLOAD ];
};

struct uart_link
{
    int (*send_byte)(struct uart_link *link, uint8_t byte );
    int (*recv_byte)(struct uart_link *link );
    uint32_t (*get_ms_tics)( struct uart_link *link );
    void *priv;
    int state;
    int rx_count;
    uint16_t rx_crc, check_crc;
    uint32_t rx_last_tics;
    struct uart_packet rx_packet;
};


#ifdef __linux__
int uart_link_create_linux( struct uart_link *link, const char* dev_name, int speed );
int uart_link_close_linux( struct uart_link *link );
#endif

#ifdef CONFIG_TARGET_ERTM14
int uart_link_create_wrpc_console( struct uart_link *link );
int uart_link_create_wrpc_suart( struct uart_link *link, struct simple_uart_device *uart_dev );
#endif

#ifdef MODULE_ERTM14_FPGA_UART // openMMC

#endif

int uart_link_reset( struct uart_link *link );
int uart_link_send( struct uart_link* link, struct uart_packet* pkt );
int uart_link_recv( struct uart_link* link, struct uart_packet **pkt, int timeout_ms );

#endif
