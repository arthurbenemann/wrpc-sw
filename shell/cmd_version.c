#include <wrc.h>
#include "shell.h"
#include "syscon.h"


extern const char *build_revision, *build_date;

#ifdef CONFIG_DEVELOPER
#define SUPPORT " (unsupported developer build)"
#else
#define SUPPORT ""
#endif

static int cmd_ver(const char *args[])
{
	int hwram = sysc_get_memsize();
	int8_t ret;
	const char *emsg="ERROR: There is no EEPROM available for";

	if (!strcasecmp(args[0], "carrier")) {
		ret=eeprom_read_board_fru(WRPC_FMC_I2C, CARRIER_EEPROM_ADR);
		if(ret<0) mprintf("%s CARRIER (0x%x). \n",emsg,CARRIER_EEPROM_ADR);
	}
	else if (!strcasecmp(args[0], "fmc"))
	{
		eeprom_read_board_fru(WRPC_FMC_I2C, FMC1_EEPROM_ADR);
		eeprom_read_board_fru(WRPC_FMC_I2C, FMC2_EEPROM_ADR);
	}
	else if (!strcasecmp(args[0], "fmc1"))
	{
		ret=eeprom_read_board_fru(WRPC_FMC_I2C, FMC1_EEPROM_ADR);
		if(ret<0) mprintf("%s FMC1 (0x%x). \n",emsg,FMC1_EEPROM_ADR);
	}
	else if (!strcasecmp(args[0], "fmc2"))
	{
		ret=eeprom_read_board_fru(WRPC_FMC_I2C, FMC2_EEPROM_ADR);
		if(ret<0) mprintf("%s FMC2 (0x%x). \n",emsg,FMC_EEPROM_ADR);
	}
	else
	{
		pp_printf("WR Core build: %s%s\n", build_revision, SUPPORT);
		pp_printf("%s", build_date); /* may be empty, or complete with \n */
		pp_printf("Built for %d kB RAM, stack is %d bytes\n",
			  CONFIG_RAMSIZE / 1024, CONFIG_STACKSIZE);
		/* hardware reports memory size, with a 16kB granularity */
		if ( hwram / 16 != CONFIG_RAMSIZE / 1024 / 16)
			pp_printf("WARNING: hardware says %ikB <= RAM < %ikB\n",
				  hwram, hwram + 16);

		//Print FRU
		eeprom_read_board_fru(WRPC_FMC_I2C, CARRIER_EEPROM_ADR);
		eeprom_read_board_fru(WRPC_FMC_I2C, FMC1_EEPROM_ADR);
		eeprom_read_board_fru(WRPC_FMC_I2C, FMC2_EEPROM_ADR);
	}


	return 0;
}

DEFINE_WRC_COMMAND(ver) = {
	.name = "ver",
	.exec = cmd_ver,
};
