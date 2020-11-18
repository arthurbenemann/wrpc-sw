#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <termios.h>
#include <fcntl.h>
#endif


#ifdef CONFIG_TARGET_ERTM14
#include "dev/console.h"
#include "dev/simple_uart.h"
#endif

#include "ertm14-uart-link.h"

#ifndef DEBUG
#define ulink_dbg(...)
#else
#ifdef __linux__
#define ulink_dbg(...) fprintf(stderr,__VA_ARGS__)
#else
#define ulink_dbg(...)
#endif
#endif

#define CON_ESCAPE_CODE 0x1b
#define CON_SWITCH_BINARY_CODE 'B'
#define CON_SWITCH_TEXT_CODE 'T'

#define CRC_POLY 0x8408

#define LINK_STATE_IDLE 0
#define LINK_STATE_SYNC 1
#define LINK_STATE_PTYPE 2
#define LINK_STATE_LEN0 3
#define LINK_STATE_LEN1 4
#define LINK_STATE_PAYLOAD 5
#define LINK_STATE_CRC0 6
#define LINK_STATE_CRC1 7

#define RX_FSM_TIMEOUT 1000 /* ms */



static uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
    int i;

    crc = crc ^ (((uint16_t)data) << 8);

    for (i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
        {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}


#ifdef CONFIG_TARGET_ERTM14

static uint32_t wrpc_get_ms_tics( struct uart_link* link )
{
    return timer_get_tics();
}

static int wrpc_console_uart_send_byte( struct uart_link* link, uint8_t b )
{
    return console_binary_send_byte( &console_uart_dev, b );
}

static int wrpc_console_uart_recv_byte( struct uart_link* link )
{
    return console_binary_recv_byte( &console_uart_dev ) ;
}

int uart_link_create_wrpc_console( struct uart_link *link )
{
    link->priv = NULL;
    link->send_byte = wrpc_console_uart_send_byte;
    link->recv_byte = wrpc_console_uart_recv_byte;
    link->get_ms_tics = wrpc_get_ms_tics;
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
    return 0;
};


static int wrpc_suart_send_byte( struct uart_link* link, uint8_t b )
{
    struct simple_uart_device *suart = (struct simple_uart_device* ) link->priv;

}

static int wrpc_suart_recv_byte( struct uart_link* link )
{
    struct simple_uart_device *suart = (struct simple_uart_device* ) link->priv;
}

int uart_link_create_wrpc_suart( struct uart_link *link, struct simple_uart_device *uart_dev )
{
    link->priv = uart_dev;
    link->send_byte = wrpc_suart_send_byte;
    link->recv_byte = wrpc_suart_recv_byte;
    link->get_ms_tics = wrpc_get_ms_tics;
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
    return 0;
};


#endif

#ifdef __linux__

struct uart_link_priv
{
    int fd;
    int esc_pending;
};

int uart_link_send_byte_raw( struct uart_link* link, uint8_t b )
{
    struct uart_link_priv *priv = (struct uart_link_priv* ) link->priv;

    int rv = write( priv->fd, &b, 1);

    ulink_dbg("TxRaw %x r %d\n", b, rv);

    return rv == 1 ? 0 : -EBUSY;
}

int uart_link_send_byte( struct uart_link* link, uint8_t b )
{
    if( b == CON_ESCAPE_CODE )
    {
        if( uart_link_send_byte_raw( link, CON_ESCAPE_CODE ) < 0 )
            return -1;
    }

    return uart_link_send_byte_raw( link, b );
}


int uart_link_recv_byte_raw( struct uart_link* link )
{
    struct uart_link_priv *priv = (struct uart_link_priv* ) link->priv;
    uint8_t b;

    int rv = read( priv->fd, &b, 1);

    return rv == 1 ? b : -1;
}

int uart_link_recv_byte( struct uart_link* link )
{
    return uart_link_recv_byte_raw( link );
}


int uart_link_set_binary( struct uart_link* link )
{
    struct uart_link_priv *priv = (struct uart_link_priv* ) link->priv;

    int retries;

    for( retries = 0; retries < 3; retries++ )
    {
        if( uart_link_send_byte_raw( link, CON_ESCAPE_CODE ) < 0 )
            return -1;

        if( uart_link_send_byte_raw( link, CON_SWITCH_BINARY_CODE ) < 0)
            return -1;
    }

    return 0;
}


uint32_t uart_link_get_ms_tics( struct uart_link *link )
{
    struct timezone tz = {0,0};
    struct timeval tv;

    gettimeofday(&tv, &tz);

    uint64_t tics = (uint64_t) tv.tv_sec * 1000ULL + tv.tv_usec / 1000;

    return (uint32_t) tics;
}

int uart_link_create_linux( struct uart_link *link, const char* dev_name, int speed )
{
    struct uart_link_priv *priv = malloc( sizeof( struct uart_link_priv ));
    struct termios t;
    int fd;
    int spd;

    if(!priv)
        return -ENOMEM;

    switch(speed)
    {
        case 921600: spd=B921600; break;
        case 230400: spd=B230400; break;
        case 460800: spd=B460800; break;
        case 115200: spd=B115200; break;
        case 57600: spd=B57600; break;
        case 38400: spd=B38400; break;
        case 19200: spd=B19200; break;
        case 9600: spd=B9600; break;
        default: return -EINVAL;
    }

    fd = open (dev_name, O_RDWR | O_NONBLOCK | O_SYNC);

    if(fd<0)
        return fd;

    tcgetattr (fd, &t);
    t.c_iflag = IGNBRK | IGNPAR;
    t.c_oflag = t.c_lflag = t.c_line = 0;
    t.c_cflag = CSTOPB | CS8 | CREAD |  CLOCAL | HUPCL | spd;
    tcsetattr (fd, TCSAFLUSH, &t);

    priv->fd = fd;
    link->send_byte = uart_link_send_byte;
    link->recv_byte = uart_link_recv_byte;
    link->get_ms_tics = uart_link_get_ms_tics;
    link->state = LINK_STATE_IDLE;
    link->priv = priv;
    link->rx_last_tics = 0;

    return uart_link_set_binary( link );
};

int uart_link_close_linux( struct uart_link *link )
{
    struct uart_link_priv *priv = (struct uart_link_priv* ) link->priv;
    close( priv->fd );
    return 0;
}

#endif


static uint16_t crc16(unsigned char *buf, int len)
{
    int i;
    uint16_t cksum;
    cksum = 0;

    for (i = 0; i < len; i++) {
        cksum = crc_xmodem_update(cksum, buf[i]);
    }
    return cksum;
}

int uart_link_reset( struct uart_link *link )
{
    link->state = LINK_STATE_IDLE;
    link->rx_last_tics = 0;
}

int uart_link_send( struct uart_link* link, struct uart_packet* pkt )
{
    uint8_t  buf[ ERTM14_MAX_UART_LINK_PAYLOAD + 16 ];
    uint16_t crc = 0, i;

    buf[0] = 0x55;
    buf[1] = 0xaa;
    buf[2] = pkt->ptype;
    buf[3] = (pkt->length >> 8) & 0xff;
    buf[4] = (pkt->length & 0xff);

    if(pkt->length > 0)
        memcpy(buf+5, pkt->payload, pkt->length);

    crc = crc16(buf, pkt->length+5);

    buf[pkt->length+5] = (crc >> 8);
    buf[pkt->length+6] = (crc & 0xff);

    for (i = 0; i < pkt->length+7; i++)
    {
        int res = link->send_byte( link, buf[i] );
        if ( res < 0 )
            return res;
    }

    return 0;
}

#define RX_FSM_PACKET_ERROR -2
#define RX_FSM_NO_DATA -1
#define RX_FSM_NEED_DATA 0
#define RX_FSM_GOT_PACKET 1

static int recv_fsm( struct uart_link* link, struct uart_packet **pkt )
{
    int rx_byte = link->recv_byte( link );

    uint32_t current_tics = link->get_ms_tics( link );

    if( current_tics - link->rx_last_tics > RX_FSM_TIMEOUT )

    if( rx_byte < 0 )
        return RX_FSM_NO_DATA;

    ulink_dbg( "Rx %x state %d\n", rx_byte, link->state );


    switch( link->state )
    {
        case LINK_STATE_IDLE:
            if( rx_byte == 0x55 )
            {
                link->state = LINK_STATE_SYNC;
                link->check_crc = crc_xmodem_update( 0, 0x55 );
            }
            break;

        case LINK_STATE_SYNC:
            if( rx_byte == 0xaa )
            {
                link->state = LINK_STATE_PTYPE;
                link->check_crc = crc_xmodem_update( link->check_crc, 0xaa );
            }
            else if (rx_byte == 0x55)
                link->state = LINK_STATE_SYNC;
            else
                link->state = LINK_STATE_IDLE;
            break;

        case LINK_STATE_PTYPE:
            link->rx_packet.ptype = rx_byte;
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->state = LINK_STATE_LEN0;
            break;

        case LINK_STATE_LEN0:
            link->rx_packet.length = (rx_byte << 8);
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->state = LINK_STATE_LEN1;
            break;

        case LINK_STATE_LEN1:
            link->rx_packet.length |= rx_byte;
            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);
            link->rx_count = 0;
            
            if( link->rx_packet.length == 0 )
                link->state = LINK_STATE_CRC0;
            else
                link->state = LINK_STATE_PAYLOAD;

            
            break;

        case LINK_STATE_PAYLOAD:
            if( link->rx_count == link->rx_packet.length - 1 )
            {
                link->state = LINK_STATE_CRC0;
            }

            link->check_crc = crc_xmodem_update( link->check_crc, rx_byte);

            if( link->rx_count < ERTM14_MAX_UART_LINK_PAYLOAD )
                link->rx_packet.payload[ link->rx_count++ ] = rx_byte;
            break;

        case LINK_STATE_CRC0:
            link->rx_crc = (rx_byte << 8);
            link->state = LINK_STATE_CRC1;
            break;

        case LINK_STATE_CRC1:
        {
            link->rx_crc |= rx_byte;
            link->state = LINK_STATE_IDLE;

            if (link->rx_count != link->rx_packet.length )
            {            
               // blink(1);
                return RX_FSM_PACKET_ERROR;
            }
            else if (link->rx_crc != link->check_crc )
            {
                //blink(2);
                return RX_FSM_PACKET_ERROR;
            }
            else
            {
                //blink(0);

                *pkt = &link->rx_packet;
                return RX_FSM_GOT_PACKET;
            }
        }
    }

    return RX_FSM_NEED_DATA;
}

int uart_link_recv( struct uart_link* link, struct uart_packet **pkt, int timeout_ms )
{
    uint32_t start_tics = link->get_ms_tics( link );

    for(;;)
    {
        int ret = recv_fsm( link, pkt );

        if ( timeout_ms == 0 )
            return ret;
        else if( ret == RX_FSM_PACKET_ERROR || ret == RX_FSM_GOT_PACKET )
            return ret;
        else { // check timeout
            if( link->get_ms_tics( link ) - start_tics >= timeout_ms )
            {
                ulink_dbg( "Rx timeout expired\n");
                uart_link_reset( link );
                return -ECANCELED;
            } else {
                #ifdef __linux__
                    usleep(1000);
                #endif
            }
        }
    }

    return 0;
}


#ifdef __linux__

#define ERTM14_UART_PTYPE_PING 1
#define ERTM14_UART_PTYPE_SNMP_REQ 2
#define ERTM14_UART_PTYPE_SNMP_RESP 3
#define ERTM14_UART_PTYPE_MMC_STATUS_REQ 4

int main()
{
    struct uart_link link;

    int rv = uart_link_create_linux( &link, "/dev/ttyUSB2", 921600 );

    for(;;)
    {
        fprintf(stderr,"Ping!\n");
        struct uart_packet pkt;
        pkt.ptype = ERTM14_UART_PTYPE_PING;
        pkt.length = 0;

        uart_link_send( &link, &pkt );

        struct uart_packet *rx_pkt;

        int retries = 0;

        int stat = uart_link_recv( &link, &rx_pkt, 1000 );
        if( stat > 0)
        {
            fprintf(stderr,"Pong [%d]\n", rx_pkt->length);
        }
    }

    return 0;

}
void blink( int id )
{
    
}

#endif