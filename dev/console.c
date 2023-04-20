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

#include "pp-printf.h"
#include "board.h"
#include "dev/simple_uart.h"
#include "dev/console.h"
#include "lib/syslog.h"
#include <netconsole.h>

static int puts_direct = 0;

#define CON_STATE_IDLE 0
#define CON_STATE_ESC_PENDING 1 // previous char was escape, waiting for control code
#define CON_STATE_ESC_FLUSH 2  // previous char was an unrecognized escape sequence, pass both to the user

struct console_uart_priv_data
{
    struct simple_uart_device uart_dev;
    uint8_t state;
    uint8_t prev_char;
    void (*mode_switch_hook)( int is_binary );
};

static struct console_uart_priv_data console_uart_priv;
#ifdef ERTM14_SECONDARY_DEBUG_UART
static struct console_uart_priv_data console_uart_priv_2nd;
#endif
struct console_device console_uart_dev, console_uart_2nd;
struct console_device* console_devs[BOARD_MAX_CONSOLE_DEVICES];
static struct console_device console_netconsole_dev;
static struct console_device console_syslog_dev;

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
        } else {
            return rx_char;
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
                if( priv->mode_switch_hook )
                    priv->mode_switch_hook( 0 );
                return -1;
            case CON_SWITCH_BINARY_CODE: // switch to binary mode
                dev->flags &= ~CONSOLE_FLAGS_MODE_TTY;
                dev->flags |= CONSOLE_FLAGS_MODE_BINARY;
                if( priv->mode_switch_hook )
                    priv->mode_switch_hook( 1 );
                return -1;
            default:
                priv->state = CON_STATE_ESC_FLUSH;
                priv->prev_char = rx_char;
                return -1;
        }
        priv->state = CON_STATE_IDLE;
    }
    return rx_char;
}

static int con_uart_put_string(struct console_device* dev, const char *s)
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    char c;
    int count = 0;

    // binary mode uses different API
    if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        return 0;

    while ( (c = *s++) != 0 )
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
    if( dev->flags & CONSOLE_FLAGS_MODE_BINARY )
        return 0;

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
    return 0;
}


static int con_ipmi_put_string(struct console_device* dev, const char *s)
{
    struct console_ipmi_priv_data* priv = (struct console_ipmi_priv_data*) dev->priv;
    int c, n = 0;
    while( (c = *s++) != 0 )
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

void console_ipmi_init( void )
{
    console_ipmi_dev.priv = &console_ipmi_priv;
    console_ipmi_dev.get_char = con_ipmi_getc;
    console_ipmi_dev.put_string = con_ipmi_put_string;
    rbuf_init(&console_ipmi_priv.rx_buf, IPMI_CON_RX_BUF_SIZE, console_ipmi_priv.rx_buf_mem);
    rbuf_init(&console_ipmi_priv.tx_buf, IPMI_CON_TX_BUF_SIZE, console_ipmi_priv.tx_buf_mem);
    console_register_device( &console_ipmi_dev );
}

#endif

static int con_netconsole_getc(struct console_device* dev)
{
	return netconsole_read_byte();
}

static int con_netconsole_put_string(struct console_device* dev, const char *s)
{
	return netconsole_write_string(s);
}

static void console_netconsole_init(void)
{
	console_netconsole_dev.get_char = con_netconsole_getc;
	console_netconsole_dev.put_string = con_netconsole_put_string;
	console_register_device( &console_netconsole_dev );
}

static int con_syslog_put_string(struct console_device* dev, const char *s)
{
	return syslog_puts(s);
}


static void console_syslog_init(void)
{
	/* no get_char for syslog! */
	console_syslog_dev.put_string = con_syslog_put_string;
	console_register_device(&console_syslog_dev);
}

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


int console_getc(void)
{
    int i;

    for(i = 0; i < BOARD_MAX_CONSOLE_DEVICES; i++)
    {
        struct console_device *con = console_devs[i];
        if(!con)
            continue;
        /* continue if no get_char function implemented */
        if (!con->get_char)
            continue;
        int b = con->get_char( con );

        if( b > 0 )
            return b;
    }

    return -1;
}

void console_set_mode_switch_hook( struct console_device *dev, void (*callback)(int) )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;
    priv->mode_switch_hook = callback;
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
    console_uart_priv.mode_switch_hook = NULL;
    console_register_device( &console_uart_dev );

#ifdef CONFIG_IPMI_CONSOLE
    console_ipmi_init();
#endif

    if (HAS_NETCONSOLE)
	console_netconsole_init();

    if (HAS_PUTS_SYSLOG)
	console_syslog_init();

#ifdef ERTM14_SECONDARY_DEBUG_UART
    // hack: there's a second UART attached to the console available on the J11 pins 2 & 3.
    // This is meant to help debugging the UART link (which uses the primary front panel USB console uart...)
    suart_init( &console_uart_priv_2nd.uart_dev, BASE_ERTM14_DEBUG_UART, CONSOLE_UART_BAUDRATE );

    console_uart_2nd.flags = CONSOLE_FLAGS_MODE_TTY | CONSOLE_FLAGS_INSERT_CRLF;
    console_uart_2nd.priv = &console_uart_priv_2nd;
    console_uart_2nd.get_char = con_uart_getc;
    console_uart_2nd.put_string = con_uart_put_string;

    console_uart_priv_2nd.prev_char = 0;
    console_uart_priv_2nd.state = CON_STATE_IDLE;
    console_uart_priv_2nd.mode_switch_hook = NULL;
    console_register_device( &console_uart_2nd );

    pp_printf("Console UART FIFO:: %d\n", suart_is_fifo_supported( &console_uart_priv.uart_dev ) );
    pp_printf("Debug UART FIFO:: %d\n", suart_is_fifo_supported( &console_uart_priv_2nd.uart_dev ) );
#endif
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

int console_binary_send_byte( struct console_device *dev, uint8_t b )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if( dev->flags & CONSOLE_FLAGS_MODE_TTY )
        return 0;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    suart_write_byte( &priv->uart_dev, b );

    return 0;
}

int console_binary_recv_byte( struct console_device *dev )
{
    struct console_uart_priv_data* priv = (struct console_uart_priv_data*) dev->priv;

    if( dev->flags & CONSOLE_FLAGS_MODE_TTY )
        return -1;

    int has_fifo = suart_is_fifo_supported( &priv->uart_dev );

    if( !has_fifo )
        return -ENODEV;

    int rx_byte = con_rx_internal( dev );

    return rx_byte;
}
