/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Author: Stefan Rauch <s.rauch@gsi.de>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 * Author: Alessandro Rubini <rubini@gnudd.com>
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <etherbone.h>

#include <w1.h>

#define W1_VENDOR	0xce42		/* CERN */
#define W1_DEVICE	0x779c5443	/* WR-Periph-1Wire */

char *prgname;
int verbose;

eb_address_t BASE_ONEWIRE;
eb_device_t device;
extern struct w1_bus wrpc_w1_bus;


static int read_w1(int w1base)
{
	struct w1_dev *d;
	int i, temp;
  
  wrpc_w1_bus.detail = w1base; /* choose portnum */
	wrpc_w1_init();
	w1_scan_bus(&wrpc_w1_bus);

	if (verbose) { /* code borrowed from dev/w1.c -- "w1" shell command */
		for (i = 0; i < W1_MAX_DEVICES; i++) {
			d = wrpc_w1_bus.devs + i;
			if (d->rom) {
        temp = w1_read_temp(wrpc_w1_bus.devs + i, 0);
				fprintf(stderr, "device %i: %08x%08x", i, (int)(d->rom >> 32), (int)d->rom);
        if ((d->rom & 0xff) == 0x42 || (d->rom & 0xff) == 0x28)
          fprintf(stderr, " temp: %d.%04d",  temp >> 16, (int)((temp & 0xffff) * 10 * 1000 >> 16));
        fprintf(stderr, "\n");
      }
		}
	}
	return 0;
}

/*
 * What follows is mostly generic, should be librarized in a way
 */


static int help(void)
{
	fprintf(stderr, "%s: Use: \"%s [-v] [-i <index>] <device>\n",
		prgname, prgname);
	return 1;
}

static void die(const char *reason, eb_status_t status)
{
	fprintf(stderr, "%s: %s: %s\n", prgname, reason, eb_status(status));
	exit(1);
}

int main(int argc, char **argv)
{
	int c, i;
	eb_status_t status;
	eb_socket_t socket;
	struct sdb_device sdb[10];;
	char *tail;

	prgname = argv[0];
	i = -1;

	while ((c = getopt(argc, argv, "i:v")) != -1) {
		switch(c) {
		case 'i':
			i = strtol(optarg, &tail, 0);
			if (*tail != 0) {
				fprintf(stderr, "Specify a proper number, not '%s'!\n", optarg);
				exit(1);
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
			exit(help());
		}
	}
	if (optind != argc - 2)
		exit(help());

	if ((status = eb_socket_open(EB_ABI_CODE, 0, EB_DATAX|EB_ADDRX, &socket)) != EB_OK)
		die("eb_socket_open", status);

	if ((status = eb_device_open(socket, argv[optind], EB_DATAX|EB_ADDRX, 3, &device)) != EB_OK)
		die(argv[optind], status);
	
	/* Find the W1 device */
	c = sizeof(sdb) / sizeof(struct sdb_device);
	if ((status = eb_sdb_find_by_identity(device, W1_VENDOR, W1_DEVICE, &sdb[0], &c)) != EB_OK)
		die("eb_sdb_find_by_identity", status);
	
	if (i == -1) {
		if (c > 1) {
			fprintf(stderr, "Found %d 1wire controllers on that device; pick one with -i #\n", c);
			exit(1);
		} else {
			i = 0;
		}
	}
	if (i >= c) {
		fprintf(stderr, "Could not find 1wire controller #%d on that device (%d total)\n", i, c);
		exit(1);
	}
	
	BASE_ONEWIRE = sdb[i].sdb_component.addr_first;
	
	return read_w1(atoi(argv[optind + 1]));
}
