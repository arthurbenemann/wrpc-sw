/* SoftPLL debug proxy

 Reads out the debug FIFO datastream from the SoftPLL and proxies it
 via TCP connection to the application running on an outside host, where
 the can be plotted, analyzed, etc.

 The debug stream contains run-time signals coming in/out the SoftPLL
 - for example, the phase/frequency errors on each channel, DAC drive
 values, phase tags.

 Todo: poll the hardware FIFO through a driver with interrupt support
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#include <fcntl.h>
#include <hw/softpll_regs.h> //TODO:check

#include <libdevmap.h>


#define BASE_SOFTPLL 	0x20200

uint64_t get_tics()
{
	struct timezone tz={0,0};
	struct timeval tv;
	gettimeofday(&tv,&tz);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static struct mapping_desc *wrc = NULL;


static inline uint32_t readl( volatile void* where )
{
	return *(volatile uint32_t *) ( where );
}

uint32_t spll_read(uint32_t reg)
{
	uint32_t v= readl( wrc->base + BASE_SOFTPLL + reg );
	return v;
}

static struct SPLL_WB _spll_regs;

#define REG(x) ((void*)(&_spll_regs.x) - (void*)&_spll_regs)


int run_dump(int dump_to_file, const char *filename, int duration_sec)
{
	uint64_t duration_usec = duration_sec * 1000000ULL;
	if(!dump_to_file)
		return 0;

	FILE *f = fopen(filename, "wb");

	uint64_t t_start = get_tics();

	for(;;)
	{
		uint32_t csr =	spll_read(REG(DFR_HOST_CSR));

		if(csr & SPLL_DFR_HOST_CSR_EMPTY) break;

		volatile int value = spll_read(REG(DFR_HOST_R0));
		volatile int seq = spll_read(REG(DFR_HOST_R1)) & 0xffff;
	}

	printf("Dumping for %d seconds...\n", duration_sec);

	while( (get_tics() - t_start) < duration_usec )
	{
		uint32_t csr =	spll_read(REG(DFR_HOST_CSR));

		if(csr & SPLL_DFR_HOST_CSR_EMPTY) continue;

		uint32_t value = spll_read(REG(DFR_HOST_R0));
		uint32_t seq_id = spll_read(REG(DFR_HOST_R1)) & 0xffff;

		fwrite(&value, sizeof(value), 1, f );
		fwrite(&seq_id, sizeof(value), 1, f );
	}

	fclose(f);
}

int main(int argc, char *argv[])
{
	int ret = 0, c;
	struct mapping_args *map_args;
	int dump_to_file = 0;
	char dump_filename[1024];
	int dump_duration = 10;

	map_args = dev_parse_mapping_args(argc, argv);
	if (!map_args) {
		fprintf(stderr,"Mapping args expected\n");
		fprintf(stderr, "%s\n", dev_mapping_help());
		return -1;
	}

	wrc = dev_map(map_args, 0x30000);
	if (!wrc) {
		fprintf(stderr, "%s: wrc mmap() failed: %s\n", argv[0],
			strerror(errno));
		free(map_args);
		return -1;
	}

	optind = 0;

	/* Parse specific args */
	while ((c = getopt (argc, argv, "d:t:")) != -1) {
		printf("PARSE '%c'\n", c);
		switch (c) {
		case 'd':
			dump_to_file = 1;
			strncpy( dump_filename,  optarg, sizeof(dump_filename) );
			break;
		case 't':
			dump_duration = atoi(optarg);
			printf("D: %d\n", dump_duration);
			break;
		case 'h':
			break;
		case '?':
			break;
		}
	}

	printf("D %d d2 %d\n",dump_to_file, dump_duration );

	run_dump( dump_to_file, dump_filename, dump_duration );

	dev_unmap(wrc);
	return (ret) ? -1 : 0;
}
