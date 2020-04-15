/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <wrc.h>
#include <shell.h>

static int cmd_help(const char *args[])
{
	pp_printf("Available commands:\n");
	shell_list_cmds();
	return 0;
}

DEFINE_WRC_COMMAND(help) = {
	.name = "help",
	.exec = cmd_help,
};
