// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2020 CERN (www.cern.ch)
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <libgen.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libdevmap.h>

static char *name;
static const char * const wrpc_firmware_version_s = "version: " __GIT_VER__;

static void help(void)
{
	fprintf(stderr, "Usage: %s [options]\n", name);
	fputs(dev_mapping_help(), stderr);
	fputs("\t-V  print version\n", stderr);
	fputs("\t-h  print help\n", stderr);
	fputs("\t-b  soft-CPU program binary\n", stderr);
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

static char *load_binary_file(const char *filename, size_t *size)
{
	int i;
	struct stat stbuf;
	char *buf;
	FILE *f;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "%s: %s\n", filename, strerror(errno));
		return NULL;
	}
	if (fstat(fileno(f), &stbuf) < 0) {
		fprintf(stderr, "%s: %s\n", filename, strerror(errno));
		fclose(f);
		return NULL;
	}

	if (!S_ISREG(stbuf.st_mode)) {
		fprintf(stderr, "%s: not a regular file\n", filename);
		fclose(f);
		return NULL;
	}

	buf = malloc(stbuf.st_size);
	if (!buf) {
		fprintf(stderr, "loading %s: %s\n", filename, strerror(errno));
		fclose(f);
		return NULL;
	}

	i = fread(buf, 1, stbuf.st_size, f);
	fclose(f);
	if (i < 0) {
		fprintf(stderr, "reading %s: %s\n", filename, strerror(errno));
		free(buf);
		return NULL;
	}
	if (i != stbuf.st_size) {
		fprintf(stderr, "%s: short read\n", filename);
		free(buf);
		return NULL;
	}

	*size = stbuf.st_size;
	return buf;
}

static int soft_cpu_program(char *binary)
{
	char *buf;
	size_t size;

	buf = load_binary_file(binary, &size);
	if (!buf)
		return -1;

        free(buf);
	return 0;
}

int main(int argc, char **argv)
{
	struct mapping_args *map_args;
	char *binary;
	int c;
	int err;

	name = strndup(basename(argv[0]), 64);
	if (!name)
		exit(EXIT_FAILURE);
	err = atexit(cleanup);
	if (err)
		exit(EXIT_FAILURE);

	while ((c = getopt (argc, argv, "hVb:")) != -1)
	{
		switch(c) {
		case 'h':
			help();
			exit(EXIT_SUCCESS);
		case 'V':
			version();
			exit(EXIT_SUCCESS);
		case 'b':
			binary = optarg;
			break;
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

	err = soft_cpu_program(binary);
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);

err_args:
	exit(EXIT_FAILURE);
}
