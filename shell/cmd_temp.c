/* 	Command: temp_gui
		Arguments: none

		Description: launches the Temperature Monitor */

#include "shell.h"

static int cmd_temp_gui(const char *args[])
{
	wrc_ui_mode = UI_SCAN_TEMP;
	return 0;
}

DEFINE_WRC_COMMAND(temp_gui) = {
	.name = "temp_gui",
	.exec = cmd_temp_gui,
};
