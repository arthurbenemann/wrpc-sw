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
	if (!args[0])
	{
		pp_printf("Command expected: format, ls\n");
		return 0;
	}
	
	if (!strcasecmp(args[0], "format") || !strcasecmp(args[0], "fs")) {
		uint32_t base = 0;
		if( !args[1] ) {
			pp_printf("Formatting using default location\n");
			storage_sdbfs_format( &wrc_storage_dev, base, 0 );
		} else {
			base = atoi(args[1]);
			pp_printf("Formatting using custom location 0x%X\n", base);
			storage_sdbfs_format( &wrc_storage_dev, base, 1 );
		}
		return 0;
	} else if ( !strcasecmp( args[0], "fse")) {
		uint32_t base = 0;
		if( !args[1] ) {
			pp_printf("Erasing using default location\n");
			storage_sdbfs_erase( &wrc_storage_dev, base, 0 );
		} else {
			base = atoi(args[1]);
			pp_printf("Erasing using custom location 0x%X\n", base);
			storage_sdbfs_erase( &wrc_storage_dev, base, 1 );
		}
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
