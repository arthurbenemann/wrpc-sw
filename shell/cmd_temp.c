#include <wrc.h>
#include <wrpc.h>
#include "shell.h"
#include "board.h"

static int cmd_temp(const char *args[]){
    pp_printf("Temperature : %u  degrees Celcius\n", temp_poll());
    return 0;
}

DEFINE_WRC_COMMAND(temp) = {
	.name = "temp",
	.exec = cmd_temp,
};