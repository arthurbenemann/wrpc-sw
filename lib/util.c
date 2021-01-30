/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <wrc.h>

/* cut from libc sources */

#define 	YEAR0   1900
#define 	EPOCH_YR   1970
#define 	SECS_DAY   (24L * 60L * 60L)
#define 	LEAPYEAR(year)   (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define 	YEARSIZE(year)   (LEAPYEAR(year) ? 366 : 365)
#define 	FIRSTSUNDAY(timp)   (((timp)->tm_yday - (timp)->tm_wday + 420) % 7)
#define 	FIRSTDAYOF(timp)   (((timp)->tm_wday - (timp)->tm_yday + 420) % 7)
#define 	TIME_MAX   ULONG_MAX
#define 	ABB_LEN   3

static const char *_days[] = {
	"Sun", "Mon", "Tue", "Wed",
	"Thu", "Fri", "Sat"
};

static const char *_months[] = {
	"Jan", "Feb", "Mar",
	"Apr", "May", "Jun",
	"Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec"
};

static const int _ytab[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

char *format_time(uint64_t sec, int format)
{
	struct tm t;
	static char buf[64];
	unsigned long dayclock, dayno;
	int year = EPOCH_YR;

	dayclock = (unsigned long)sec % SECS_DAY;
	dayno = (unsigned long)sec / SECS_DAY;

	t.tm_sec = dayclock % 60;
	t.tm_min = (dayclock % 3600) / 60;
	t.tm_hour = dayclock / 3600;
	t.tm_wday = (dayno + 4) % 7;	/* day 0 was a thursday */
	while (dayno >= YEARSIZE(year)) {
		dayno -= YEARSIZE(year);
		year++;
	}
	t.tm_year = year - YEAR0;
	t.tm_yday = dayno;
	t.tm_mon = 0;
	while (dayno >= _ytab[LEAPYEAR(year)][t.tm_mon]) {
		dayno -= _ytab[LEAPYEAR(year)][t.tm_mon];
		t.tm_mon++;
	}
	t.tm_mday = dayno + 1;
	t.tm_isdst = 0;

	switch(format) {
	case TIME_FORMAT_LEGACY:
	default:
		sprintf(buf, "%s, %s %d, %d, %02d:%02d:%02d", _days[t.tm_wday],
			_months[t.tm_mon], t.tm_mday, t.tm_year + YEAR0,
			t.tm_hour, t.tm_min, t.tm_sec);
		break;
	case TIME_FORMAT_SYSLOG:
		sprintf(buf, "%s %2d %02d:%02d:%02d", _months[t.tm_mon],
			t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		break;
	case TIME_FORMAT_SORTED:
		sprintf(buf, "%4d-%02d-%02d-%02d:%02d:%02d",
			t.tm_year + YEAR0, t.tm_mon + 1, t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec);
		break;
	}

	return buf;
}

void cprintf(int color, const char *fmt, ...)
{
	va_list ap;
	pp_printf("\e[0%d;3%dm", color & C_DIM ? 2 : 1, color & 0x7f);
	va_start(ap, fmt);
	pp_vprintf(fmt, ap);
	va_end(ap);
}

void pcprintf(int row, int col, int color, const char *fmt, ...)
{
	va_list ap;
	pp_printf("\e[%d;%df", row, col);
	pp_printf("\e[0%d;3%dm", color & C_DIM ? 2 : 1, color & 0x7f);
	va_start(ap, fmt);
	pp_vprintf(fmt, ap);
	va_end(ap);
}

void pprintf(int row, int col, const char *fmt, ...)
{
	va_list ap;
	pp_printf("\e[%d;%df", row, col);
	va_start(ap, fmt);
	pp_vprintf(fmt, ap);
	va_end(ap);
}

void __debug_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pp_vprintf(fmt, ap);
	va_end(ap);
}


void term_clear(void)
{
	pp_printf("\e[2J\e[1;1H");
}

void term_clear_to_end(void)
{
	pp_printf("\e[J");
}

int tmo_init(timeout_t *tmo, uint32_t milliseconds)
{
	tmo->start_tics = timer_get_tics();
	tmo->timeout = milliseconds;
	return 0;
}

int tmo_restart(timeout_t *tmo)
{
	tmo->start_tics = timer_get_tics();
	return 0;
}

int tmo_expired(timeout_t *tmo)
{
	return (timer_get_tics() - tmo->start_tics > tmo->timeout);
}


const char *fromhex64(const char *hex, int64_t *v)
{
	int64_t o = 0;
	int sign = 1;

	if (hex && *hex == '-') {
		sign = -1;
		hex++;
	}
	for (; hex && *hex; ++hex) {
		if (*hex >= '0' && *hex <= '9') {
			o = (o << 4) + (*hex - '0');
		} else if (*hex >= 'A' && *hex <= 'F') {
			o = (o << 4) + (*hex - 'A') + 10;
		} else if (*hex >= 'a' && *hex <= 'f') {
			o = (o << 4) + (*hex - 'a') + 10;
		} else {
			break;
		}
	}

	*v = o * sign;
	return hex;
}

const char *fromhex(const char *hex, int *v)
{
	const char *ret;
	int64_t v64;

	ret = fromhex64(hex, &v64);
	*v = (int)v64;
	return ret;
}

const char *fromdec(const char *dec, int *v)
{
	int o = 0, sign = 1;

	if (dec && *dec == '-') {
		sign = -1;
		dec++;
	}
	for (; dec && *dec; ++dec) {
		if (*dec >= '0' && *dec <= '9') {
			o = (o * 10) + (*dec - '0');
		} else {
			break;
		}
	}

	*v = o * sign;
	return dec;
}
