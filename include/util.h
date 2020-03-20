/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __UTIL_H
#define __UTIL_H

#include <stdint.h>

/* Color codes for cprintf()/pcprintf() */
#define C_DIM 0x80
#define C_WHITE 7
#define C_GREY (C_WHITE | C_DIM)
#define C_RED 1
#define C_GREEN 2
#define C_BLUE 4

/* Return TAI date/time in human-readable form. Non-reentrant. */
char *format_time(uint64_t sec, int format);
#define TIME_FORMAT_LEGACY 0
#define TIME_FORMAT_SYSLOG 1
#define TIME_FORMAT_SORTED 2

typedef struct
{
    uint32_t start_tics;
    uint32_t timeout;
} timeout_t;

/* Color printf() variant. */
void cprintf(int color, const char *fmt, ...);

/* Color printf() variant, sets curspor position to (row, col) too. */
void pcprintf(int row, int col, int color, const char *fmt, ...);

void __debug_printf(const char *fmt, ...);

/* Clears the terminal scree. */
void term_clear(void);

int tmo_init(timeout_t *tmo, uint32_t milliseconds);
int tmo_restart(timeout_t *tmo);
int tmo_expired(timeout_t *tmo);

static inline int within_range(int x, int minval, int maxval, int wrap)
{
    int rv;

    //printf("min %d max %d x %d ", minval, maxval, x);

    while (maxval >= wrap)
        maxval -= wrap;

    while (maxval < 0)
        maxval += wrap;

    while (minval >= wrap)
        minval -= wrap;

    while (minval < 0)
        minval += wrap;

    while (x < 0)
        x += wrap;

    while (x >= wrap)
        x -= wrap;

    if (maxval > minval)
        rv = (x >= minval && x <= maxval) ? 1 : 0;
    else
        rv = (x >= minval || x <= maxval) ? 1 : 0;

    return rv;
}


#endif
