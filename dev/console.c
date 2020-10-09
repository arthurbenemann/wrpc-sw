/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012-2020 CERN (www.cern.ch)
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "board.h"
#include "dev/simple_uart.h"
#include "dev/console.h"

static int puts_direct = 0;

#define CON_STATE_IDLE 0
#define CON_STATE_ESC_PENDING 1 // previous char was escape, waiting for control code
#define CON_STATE_ESC_FLUSH 2  // previous char was an unrecognized escape sequence, pass both to the user

struct console_uart_priv_data
{
    struct simple_uart_device uart_dev;
    uint8_t state;
    uint8_t prev_char;
};

static struct console_uart_priv_data console_uart_priv;
struct console_device console_uart_dev;
struct console_device* console_devs[BOARD_MAX_CONSOLE_DEVICES];

#define CON_ESCAPE_CODE 0x1b
#define CON_SWITCH_BINARY_CODE 'B'
#define CON_SWITCH_TEXT_CODE 'T'

static int con_rx_internal(struct console_device* dev)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if ( priv->state == CON_STATE_ESC_FLUSH )
    {
        priv->state = CON_STATE_IDLE;
        return priv->prev_char;
    }

    int rx_char = suart_read_byte( &priv->uart_dev );

    if( rx_char < 0 )
        return rx_char;

    if( priv->state == CON_STATE_IDLE )
    {
        if( rx_char == CON_ESCAPE_CODE )
        {
            priv->state = CON_STATE_ESC_PENDING;
            return -1;
        }
    }
    else if( priv->state == CON_STATE_ESC_PENDING )
    {
        switch( rx_char )
        {
            case CON_ESCAPE_CODE: // double escape = actual esc
                return CON_ESCAPE_CODE;
            case CON_SWITCH_TEXT_CODE: // switch to tty mode
                dev->flags &= ~CONSOLE_FLAGS_MODE_BINARY;
                dev->flags |= CONSOLE_FLAGS_MODE_TTY;
                return -1;
            case CON_SWITCH_BINARY_CODE: // switch to binary mode
                dev->flags &= ~CONSOLE_FLAGS_MODE_TTY;
                dev->flags |= CONSOLE_FLAGS_MODE_BINARY;
                return -1;
            default:
                priv->state = CON_STATE_ESC_FLUSH;
                priv->prev_char = rx_char;
                return CON_ESCAPE_CODE;
        }
        priv->state = CON_STATE_IDLE;
    }
}

static int con_uart_put_string(struct console_device* dev, const char *s)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    char c;
    int count = 0;

    // binary mode uses different API
    if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        return 0;

    while ( c = *s++ )
    {
	    if( (dev->flags & CONSOLE_FLAGS_INSERT_CRLF) &&  c == '\n')
    		suart_write_byte(&priv->uart_dev, '\r');

        suart_write_byte(&priv->uart_dev, c);
        count++;
    }

    return count;
}

static int con_uart_getc(struct console_device* dev)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    //if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        //return 0;

    return con_rx_internal( dev );
}

void console_uart_set_crlf_mode(int on)
{
    if(on)
        console_uart_dev.flags |= CONSOLE_FLAGS_INSERT_CRLF;
    else
        console_uart_dev.flags &= ~CONSOLE_FLAGS_INSERT_CRLF;
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

    console_uart_dev.flags = CONSOLE_FLAGS_MODE_TTY | CONSOLE_FLAGS_INSERT_CRLF;
    console_uart_dev.priv = &console_uart_priv;
    console_uart_dev.get_char = con_uart_getc;
    console_uart_dev.put_string = con_uart_put_string;
    
    console_uart_priv.prev_char = 0;
    console_uart_priv.state = CON_STATE_IDLE;
    
    
    console_register_device( &console_uart_dev );

#ifdef CONFIG_IPMI_CONSOLE
    console_ipmi_init();
#endif

    pp_printf("Console UART FIFO:: %d\n", suart_is_fifo_supported( &console_uart_priv.uart_dev ) );
}

void console_force_mode( struct console_device *dev, int mode )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    dev->flags &= ~( CONSOLE_FLAGS_MODE_BINARY | CONSOLE_FLAGS_MODE_TTY );
    dev->flags |= mode;
    priv->state = CON_STATE_IDLE;
}

int console_get_mode( struct console_device *dev )
{
    return dev->flags & ( CONSOLE_FLAGS_MODE_BINARY | CONSOLE_FLAGS_MODE_TTY );
}

int console_binary_send( struct console_device *dev, void *buf, int size )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    // fixme: FIFO threshold check?

    uint8_t *ptr = (uint8_t*) buf;
    int i;
    for(i = 0; i< size; i++)
        suart_write_byte( &priv->uart_dev, ptr[i] );

    return size;
}

int console_binary_recv( struct console_device *dev, void *buf, int size )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    int rx_count = suart_poll( &priv->uart_dev );

    if( rx_count < size )
        return -EAGAIN;

    uint8_t *ptr = (uint8_t*) buf;
    int i;
    for(i = 0; i< size; i++)
        ptr[i] = suart_read_byte(&priv->uart_dev );

    return size;
}

