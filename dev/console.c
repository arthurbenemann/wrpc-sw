/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2019 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdio.h>

#include "board.h"
#include "dev/simple_uart.h"
#include "dev/console.h"

static int puts_direct = 0;

struct console_uart_priv_data
{
    struct simple_uart_device uart_dev;
};

static struct console_uart_priv_data console_uart_priv;
struct console_device console_uart_dev;

struct console_device* console_devs[BOARD_MAX_CONSOLE_DEVICES];

static int con_uart_put_string(struct console_device* dev, const char *s)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
	return suart_write_string(&priv->uart_dev, s);
}

static int con_uart_getc(struct console_device* dev)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    return suart_read_byte(&priv->uart_dev);
}

void console_uart_write_bytes( uint8_t *buf, int count )
{
    int i;
    for(i=0;i<count;i++)
        suart_write_byte( &console_uart_priv.uart_dev, buf[i] );

}

void console_uart_set_crlf_mode(int on)
{
    console_uart_priv.uart_dev.crlf_mode = on;
}

static void console_register_device( struct console_device *dev )
{
    int i;
    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
        if ( console_devs[i] == NULL )
        {
            console_devs[i] = dev;
            return;
        }
    }
}


#ifdef CONFIG_IPMI_CONSOLE

#define IPMI_CON_TX_BUF_SIZE 1024
#define IPMI_CON_RX_BUF_SIZE 128
#define IPMI_CON_RX_TIMEOUT 1000

struct ring_buffer
{
    uint8_t *data;
    int head, tail, size, count;
};

struct console_ipmi_priv_data 
{
    uint8_t tx_buf_mem[IPMI_CON_TX_BUF_SIZE];
    uint8_t rx_buf_mem[IPMI_CON_RX_BUF_SIZE];

    struct ring_buffer rx_buf;
    struct ring_buffer tx_buf;
};

struct console_device console_ipmi_dev;
struct console_ipmi_priv_data console_ipmi_priv;


static inline void rbuf_put( struct ring_buffer* buf, uint8_t c )
{
    if (buf->count >= buf->size)
		return;

    buf->data[buf->head] = c;
	buf->head++;
    buf->count++;

	if (buf->head >= buf->size)
		buf->head = 0;
}


static inline int rbuf_get( struct ring_buffer* buf )
{
    if( !buf->count )
        return -1;

	int rv = buf->data[buf->tail];

    buf->tail++;
    if (buf->tail >= buf->size)
		buf->tail = 0;
    buf->count--;

    return rv;
}

static inline int rbuf_init( struct ring_buffer *buf, int size, uint8_t *mem )
{
    buf->head = buf->tail = buf->count = 0;
    buf->size = size;
    buf->data = mem;
    return 0;
}

static inline int rbuf_full(struct ring_buffer *buf)
{
    return buf->size == buf->count;
}

static inline int rbuf_purge(struct ring_buffer *buf)
{
    buf->head = buf->tail = buf->count = 0;
}


static int con_ipmi_put_string(struct console_device* dev, const char *s)
{
    struct console_ipmi_priv_data* priv = (struct console_ipmi_priv_data*) dev->priv;
    int c, n = 0;
    while( c = *s++ )
    {
        rbuf_put( &priv->tx_buf, c );
        n++;
    }

    if( rbuf_full(&priv->tx_buf ) )
        rbuf_purge(&priv->tx_buf);

    return n;
}

static int con_ipmi_getc(struct console_device* dev)
{
    struct console_ipmi_priv_data* priv = (struct console_ipmi_priv_data*) dev->priv;
    return rbuf_get( &priv->rx_buf );
}

int console_ipmi_process_request(struct console_device* dev,  uint8_t *req, int size, uint8_t *rsp, int rsp_size )
{
    struct console_ipmi_priv_data* priv = (struct console_ipmi_priv_data*) dev->priv;

    int i;
    int cnt = priv->tx_buf.count;

    for(i = 0; i < size; i++ )
    {
        puts_direct = 1;
        //pp_printf("put %x\n", req[i] );
        puts_direct = 0;

        rbuf_put( &priv->rx_buf, req[i] );
    }

    for(i = 0; i < rsp_size; i++)
    {
        if ( priv->tx_buf.count == 0 )
            break;
        rsp[i] = rbuf_get( &priv->tx_buf );
    }

    puts_direct = 1;
    //pp_printf("ConIPMIReq in %d out %d txb %d\n", size, i, cnt);
    puts_direct = 0;

    return i;
}

void console_ipmi_init( )
{
    console_ipmi_dev.priv = &console_ipmi_priv;
    console_ipmi_dev.get_char = con_ipmi_getc;
    console_ipmi_dev.put_string = con_ipmi_put_string;
    rbuf_init(&console_ipmi_priv.rx_buf, IPMI_CON_RX_BUF_SIZE, &console_ipmi_priv.rx_buf_mem);
    rbuf_init(&console_ipmi_priv.tx_buf, IPMI_CON_TX_BUF_SIZE, &console_ipmi_priv.tx_buf_mem);
    console_register_device( &console_ipmi_dev );
}

#endif

int puts(const char *s)
{
    if( puts_direct)
    {
        return con_uart_put_string( &console_uart_dev, s );
    }

    int i, rv = 0;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
	    struct console_device *con = console_devs[i];
        if(!con)
            continue;
        rv = con->put_string( con, s);
    }

    return rv;
}


int console_getc()
{
    int i;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
        struct console_device *con = console_devs[i];
        if(!con)
            continue;
        int b = con->get_char( con );

        if( b > 0 )
            return b;
    }

    return -1;
}

void console_init()
{
    int i;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
        console_devs[i] = NULL;

    suart_init( &console_uart_priv.uart_dev, BASE_UART, CONSOLE_UART_BAUDRATE );
	console_uart_priv.uart_dev.crlf_mode = 1;
    console_uart_dev.priv = &console_uart_priv;
    console_uart_dev.get_char = con_uart_getc;
    console_uart_dev.put_string = con_uart_put_string;

    console_register_device( &console_uart_dev );

#ifdef CONFIG_IPMI_CONSOLE
    console_ipmi_init();
#endif
}
