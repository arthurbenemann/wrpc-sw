/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <string.h>
#include <errno.h>

#include "shell.h"
#include "dev/syscon.h"
#include "storage.h"
#include <dev/flash.h>

/*
 * args[1] - where to write sdbfs image (0 - Flash, 1 - I2C EEPROM,
 *		2 - 1Wire EEPROM)
 * args[2] - base address for sdbfs image in Flash/EEPROM
 * args[3] - i2c address of EEPROM or blocksize of Flash
 */

static int cmd_sdb(const char *args[])
{
	int blocksize	= 1;

	if (!args[0])
	{
		pp_printf("Command expected: format, ls\n");
		return 0;
	}
	
	if (!strcasecmp(args[0], "format")) {
		uint32_t base = 0;
		if( !args[1] )
			pp_printf("Formatting using default location\n");
		else
			base = atoi(args[1]);

		storage_sdbfs_format( &wrc_storage_dev, base );
		return 0;
	} else if ( !strcasecmp( args[0], "ls" )) {
		storage_sdbfs_list();
		return 0;
	}

	return -EINVAL;
}

DEFINE_WRC_COMMAND(sdb) = {
	.name = "sdb",
	.exec = cmd_sdb,
};
