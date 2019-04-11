/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#define SDBFS_BIG_ENDIAN
#include <libsdbfs.h>

/* The following pointers are exported */
unsigned char *BASE_MINIC[2];
unsigned char *BASE_EP[2];
unsigned char *BASE_SOFTPLL;
unsigned char *BASE_PPS_GEN;
unsigned char *BASE_SYSCON;
unsigned char *BASE_UART;
unsigned char *BASE_ONEWIRE;
unsigned char *BASE_ETHERBONE_CFG;
unsigned char *BASE_TCPIP_CFG;

/* The sdb filesystem itself */
static struct sdbfs wrc_fpga_sdb = {
	.name = "fpga-area",
	.blocksize = 1, /* Not currently used */
	.entrypoint = SDB_ADDRESS,
	.data = 0,
	.flags = SDBFS_F_ZEROBASED,
};

/* called by outside, at boot time */
void sdb_print_devices(void)
{
	struct sdb_device *d;
	int new = 1;
	while ( (d = sdbfs_scan(&wrc_fpga_sdb, new)) != NULL) {
		/*
		 * "%.19s" is not working for XINT printf, and zeroing
		 * d->sdb_component.product.record_type won't work, as
		 * the device is read straight from fpga ROM registers
		 */
		const int namesize = sizeof(d->sdb_component.product.name);
		char name[namesize + 1];

		memcpy(name, d->sdb_component.product.name, sizeof(name));
		name[namesize] = '\0';
		pp_printf("dev  0x%08lx @ %06lx, %s\n",
			  (long)(d->sdb_component.product.device_id),
			  wrc_fpga_sdb.f_offset, name);
		new = 0;
	}
}

/* To save a little size, we enumerate our vendors */
#define VID_CERN	0x0000ce42LL
#define VID_GSI		0x00000651LL
#define VID_THU		0x00746875LL

struct wrc_device {
	unsigned char **base;
	uint64_t vid;
	uint32_t did;
};

struct wrc_device devs[] = {
	{&BASE_MINIC[0],      VID_CERN,	0xab28633a},
	{&BASE_EP[0],         VID_CERN,	0x650c2d4f},
#ifdef CONFIG_DUALPORT
	{&BASE_MINIC[1],      VID_CERN,	0xa224633b},
	{&BASE_EP[1],         VID_CERN,	0x650c2d4e},
#endif
	{&BASE_SOFTPLL,       VID_CERN,	0x65158dc0},
	{&BASE_PPS_GEN,       VID_CERN,	0xde0d8ced},
	{&BASE_SYSCON,        VID_CERN,	0xff07fc47},
	{&BASE_UART,          VID_CERN,	0xe2d13d04},
	{&BASE_ONEWIRE,       VID_CERN,	0x779c5443},
	{&BASE_TCPIP_CFG,     VID_THU,	0xc0413599},
	{&BASE_ETHERBONE_CFG, VID_GSI,	0x68202b22},
};

void sdb_find_devices(void)
{
	struct wrc_device *d;
	static int done;
	int i;

	if (!done) {
		sdbfs_dev_create(&wrc_fpga_sdb);
		done++;
	}
	for (d = devs, i = 0; i < ARRAY_SIZE(devs); d++, i++) {
		*(d->base) = (void *)sdbfs_find_id(&wrc_fpga_sdb,
						    d->vid, d->did);
	}
}
