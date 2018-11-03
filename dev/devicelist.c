/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

/* The following pointers are exported */
unsigned char *BASE_MINIC;
unsigned char *BASE_EP;
unsigned char *BASE_SOFTPLL;
unsigned char *BASE_PPS_GEN;
unsigned char *BASE_SYSCON;
unsigned char *BASE_UART;
unsigned char *BASE_ONEWIRE;
unsigned char *BASE_ETHERBONE_CFG;

void sdb_print_devices(void)
{
}


void sdb_find_devices(void)
{
	BASE_MINIC = (unsigned char *) 0x20000;
	BASE_EP = (unsigned char *) 0x20100;
	BASE_SOFTPLL = (unsigned char *) 0x20200;
	BASE_PPS_GEN = (unsigned char *) 0x20300;
	BASE_SYSCON = (unsigned char *) 0x20400;
	BASE_UART = (unsigned char *) 0x20500;
	BASE_ONEWIRE = (unsigned char *) 0x20600;
	BASE_ETHERBONE_CFG = (unsigned char *) 0x20700;
}
