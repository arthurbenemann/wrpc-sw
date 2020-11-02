// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <getopt.h>

#include <libdevmap.h>

static char *name;
static const char * const wrpc_firmware_version_s = "version: " __GIT_VER__;

static void help(void)
{
	fprintf(stderr, "Usage: %s [options]\n", name);
	fputs(dev_mapping_help(), stderr);
	fprintf(stderr, "\t-V  print version\n");
	fprintf(stderr, "\t-h  print help\n");
}

static void version(void)
{
	fprintf(stdout, "%s version: %s\n", name, wrpc_firmware_version_s);
	fputs(dev_get_version(), stdout);
	fputc('\n', stdout);
}

static void cleanup(void)
{
	if (name)
		free(name);
}

int main(int argc, char **argv)
{
	struct mapping_args *map_args;
	int c;
	int err;

	name = strndup(basename(argv[0]), 64);
	if (!name)
		exit(EXIT_FAILURE);
	err = atexit(cleanup);
	if (err)
		exit(EXIT_FAILURE);

	while ((c = getopt (argc, argv, "hV")) != -1)
	{
		switch(c) {
		case 'h':
			help();
			exit(EXIT_SUCCESS);
		case 'V':
			version();
			exit(EXIT_SUCCESS);
	        default:
			break;
		}
	}
	optind = 1;
	map_args = dev_parse_mapping_args(argc, argv);
	if (!map_args) {
		help();
		goto err_args;
	}

	exit(EXIT_SUCCESS);

err_args:
	exit(EXIT_FAILURE);
}
