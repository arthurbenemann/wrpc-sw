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

static int soft_cpu_program(char *binary, struct mapping_desc *map,
			    off_t offset)
{
	char *buf;
	uint32_t *ibuf;
	size_t size;
	int i;

        buf = load_binary_file(binary, &size);
	if (!buf)
		return -1;

	dev_write32(map, 0x1deadbee, offset + 0x20400);
	while ( !(dev_read32(map, offset + 0x20400) & (1 << 28)))
		;

        ibuf = (uint32_t *) buf;
	for (i = 0; i < (size + 3) / 4; i++)
		dev_write32(map, ibuf[i], offset + (i * 4));

	sync();

	for (i = 0; i < (size + 3) / 4; i++) {
		uint32_t r = dev_read32(map, offset + i * 4);

                if (r != htonl(ibuf[i]))
		{
			fprintf(stderr, "programming error at %x "
				"(expected %08x, found %08x)\n", i*4,
				htonl(ibuf[i]), r);
			return -1;
		}
	}

	sync();

	dev_write32(map, 0x0deadbee, offset + 0x20400);

        free(buf);
	return 0;
}

#define OFFSET_NOT_SET 0xFFFFFFFF
int main(int argc, char **argv)
{
	struct mapping_args *map_args;
	struct mapping_desc *map;
	char *binary;
	int c;
	int err;
	int ret;
	off_t offset = OFFSET_NOT_SET;

	name = strndup(basename(argv[0]), 64);
	if (!name)
		exit(EXIT_FAILURE);
	err = atexit(cleanup);
	if (err)
		exit(EXIT_FAILURE);

	while ((c = getopt (argc, argv, "hVb:a:")) != -1)
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
		case 'a':
			ret = sscanf(optarg, "0x%lx", &offset);
			if (ret != 1)
				exit(EXIT_FAILURE);
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

	if (offset == OFFSET_NOT_SET) {
		fputs("Option '-a' is mandatory\n", stderr);
		exit(EXIT_FAILURE);
	}

	map = dev_map(map_args, 0x10000); // FIXME size
	if (!map) {
		fprintf(stderr, "failed to map device memory: %s\n",
			strerror(errno));
	} else {
		err = soft_cpu_program(binary, map, offset);
		dev_unmap(map);
	}
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);

err_args:
	exit(EXIT_FAILURE);
}
