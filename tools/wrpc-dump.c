#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h> /* ntohl */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <ppsi/ppsi.h>
#include <softpll_ng.h>
#include <revision.h>
#include <arch/lm32/crt0.h>

#include <dump-info.h>
#include <wrpc.h>
#include "time_lib.h"

/* We have a problem: ppsi is built for wrpc, so it has ntoh[sl] wrong */
#undef ntohl
#undef ntohs
#undef ntohll
#define ntohs(x) __do_not_use
#define ntohl(x) __do_not_use
#define ntohll(x) __do_not_use

/* create fancy macro to shorten the switch statements, assign val as a string to p */
#define ENUM_TO_P_IN_CASE(val, p) \
				case val: \
				    p = #val;\
				    break;


uint32_t endian_flag; /* from dump_info[0], lazily */

int print_labels = 1;

void print_str(char *s)
{
    if (print_labels == 0)
	return;
    printf(" (%s)", s);
}
/*
 * This picks items from memory, converting as needed. No ntohl any more.
 * Next, we'll detect the byte order from the code itself.
 */
static long long wrpc_get_64(void *p)
{
	uint64_t *p64 = p;
	uint64_t result;

	if (endian_flag == DUMP_ENDIAN_FLAG) {
		return *p64;
	}
	result = __bswap_32((uint32_t)*p64);
	result <<= 32;
	result |= __bswap_32((uint32_t)(*p64 >> 32));
	return result;
}

/* printf complains for i/l mismatch, so get i32 and l32 separately */
static long wrpc_get_l32(void *p)
{
	uint32_t *p32 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p32;
	return __bswap_32(*p32);
}

static int wrpc_get_i32(void *p)
{
	return wrpc_get_l32(p);
}

static int wrpc_get_16(void *p)
{
	uint16_t *p16 = p;

	if (endian_flag == DUMP_ENDIAN_FLAG)
		return *p16;
	return __bswap_16(*p16);
}

static uint8_t wrpc_get_8(void *p)
{
	uint8_t *p8 = p;

	return *p8;
}

void dump_one_field(void *addr, struct dump_info *info, char *info_prefix)
{
	void *p = addr + wrpc_get_i32(&info->offset);
	struct pp_time *t = p;
	struct PortIdentity *pi = p;
	struct ClockQuality *cq = p;
	TimeInterval *ti=p;
	RelativeDifference *rd=p;
	char format[16];
	char pname[128];
	int i, type, size;
	char buf[128];
	char *char_p;

	/* now, info may be in wrong-endian. so fix it */
	type = wrpc_get_i32(&info->type);
	size = wrpc_get_i32(&info->size);

	if (type == dump_type_dummy) {
		/* dummy type used to store address of e.g. complex structure.
		 * It makes no point to print such address.*/
		return;
	}

	if (info_prefix!=NULL )
		sprintf(pname, "%s.%s", info_prefix, info->name);
	else
		strcpy(pname, info->name);

	printf("%-60s ", pname); /* name includes trailing ':' */

//	printf("%3d|%2d|", wrpc_get_i32(&info->offset), size);

	/* For some (mostly enum like types) the size may vary. Check the size
	 * and assign a proper value to variable i */
	switch(type) {
	case dump_type_yes_no:
	case dump_type_yes_no_Boolean:
	case dump_type_ppi_state:
	case dump_type_ppi_state_Enumeration8:
	case dump_type_wr_config:
	case dump_type_wr_config_Enumeration8:
	case dump_type_wr_role:
	case dump_type_wr_role_Enumeration8:
	case dump_type_pp_pdstate:
	case dump_type_exstate:
	case dump_type_pp_servo_flag:
	case dump_type_pp_servo_state:
	case dump_type_wr_state:
	case dump_type_ppi_profile:
	case dump_type_ppi_proto:
	case dump_type_ppi_flag:
		if (size == 1)
			i = *(uint8_t *)p;
		else if (size == 2)
			i = wrpc_get_16(p);
		else
			i = wrpc_get_l32(p);
	}

	/* check the size of Boolean, which is declared as Enum */
	if (type == dump_type_Boolean) {
		switch(size) {
		case 1:
			type = dump_type_UInteger8;
			break;
		case 2:
			type = dump_type_UInteger16;
			break;
		case 4:
		default:
			type = dump_type_UInteger32;
			break;
		}
	}

	switch(type) {
	case dump_type_char:
		sprintf(format,"\"%%.%is\"\n", size);
		printf(format, (char *)p);
		break;
	case dump_type_bina:
		for (i = 0; i < size; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == size - 1 ? '\n' : ':');
		break;
	case dump_type_UInteger64:
		printf("%lld\n", wrpc_get_64(p));
		break;
	case dump_type_long_long:
	case dump_type_Integer64:
		printf("%lld\n", wrpc_get_64(p));
		break;
	case dump_type_uint32_t:
		printf("0x%08lx\n", wrpc_get_l32(p));
		break;
	case dump_type_Integer32:
	case dump_type_int:
		printf("%i\n", wrpc_get_i32(p));
		break;
	case dump_type_UInteger32:
	case dump_type_unsigned_long:
		printf("%li\n", wrpc_get_l32(p));
		break;
	case dump_type_unsigned_char:
	case dump_type_UInteger8:
	case dump_type_Integer8:
	case dump_type_Enumeration8:
	case dump_type_UInteger4:
	case dump_type_uint8_t:
		printf("%i\n", *(unsigned char *)p);
		break;
	case dump_type_UInteger16:
	case dump_type_Integer16:
	case dump_type_uint16_t:
	case dump_type_unsigned_short:
		printf("%i\n", wrpc_get_16(p));
		break;
	case dump_type_double:
		printf("%lf\n", *(double *)p);
		break;
	case dump_type_float:
		printf("%f\n", *(float *)p);
		break;
	case dump_type_pointer:
		if (size == 4)
			printf("%08lx\n", wrpc_get_l32(p));
		else
			printf("%016llx\n", wrpc_get_64(p));
		break;
	case dump_type_yes_no:
	case dump_type_yes_no_Boolean:
		printf("%d", i);
		if (i == 0)
			print_str("no");
		else if (i == 1)
			print_str("yes");
		else
			print_str("unknown");
		printf("\n");
		break;

	case dump_type_spll_mode:
		/* check the size of type, e.g. Boolean is not 8 bits! */
		i = wrpc_get_l32(p);

		switch(i) {
		ENUM_TO_P_IN_CASE(SPLL_MODE_GRAND_MASTER, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_FREE_RUNNING_MASTER, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_SLAVE, char_p);
		ENUM_TO_P_IN_CASE(SPLL_MODE_DISABLED, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_pp_time:
	{
		struct pp_time localt;
		localt.secs = wrpc_get_64(&t->secs);
		localt.scaled_nsecs = wrpc_get_64(&t->scaled_nsecs);

		printf("correct %i: %25s rawps: 0x%04x\n",
		       !is_incorrect(&localt),
		       timeToString(&localt,buf),
		       (int)(localt.scaled_nsecs & 0xffff)
		      );
		break;
	}

	case dump_type_ip_address:
		for (i = 0; i < 4; i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == 3 ? '\n' : ':');
		break;

	case dump_type_ClockIdentity: /* Same as binary */
		for (i = 0; i < sizeof(ClockIdentity); i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == sizeof(ClockIdentity) - 1 ? '\n' : ':');
		break;

	case dump_type_PortIdentity: /* Same as above plus port */
		for (i = 0; i < sizeof(ClockIdentity); i++)
			printf("%02x%c", ((unsigned char *)p)[i],
			       i == sizeof(ClockIdentity) - 1 ? '.' : ':');
		printf("%04x (%i)\n", wrpc_get_16(&pi->portNumber),
				      wrpc_get_16(&pi->portNumber));
		break;

	case dump_type_ClockQuality:
		printf("class %i, accuracy %02x (%i), logvariance %i\n",
		       cq->clockClass, cq->clockAccuracy, cq->clockAccuracy,
		       wrpc_get_16(&cq->offsetScaledLogVariance));
		break;
	case dump_type_TimeInterval:
		printf("%15s, ", timeIntervalToString(wrpc_get_64(ti), buf));
		printf("raw:  %15lld\n", wrpc_get_64(p));
		break;
	case dump_type_RelativeDifference:
		printf("%15s, ", relativeDifferenceToString(*rd, buf));
		printf("raw:  %15lld\n", wrpc_get_64(p));
		break;
	case dump_type_FixedDelta:
		/* FixedDelta has defined order of msb and lsb,
		 * which is different than in 64bit type (e.g. uint64_t) on host */
		printf("%lld\n", ((unsigned long long)wrpc_get_l32(p)
				  |((unsigned long long)wrpc_get_l32(p+4))<<32
				 )>>16);
		break;
	case dump_type_delay_mechanism:
		i = wrpc_get_i32(p);
		switch(i) {
		ENUM_TO_P_IN_CASE(E2E, char_p);
		ENUM_TO_P_IN_CASE(P2P, char_p);
		ENUM_TO_P_IN_CASE(COMMON_P2P, char_p);
		ENUM_TO_P_IN_CASE(SPECIAL, char_p);
		ENUM_TO_P_IN_CASE(NO_MECHANISM, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_protocol_extension:
		i = wrpc_get_i32(p);
		switch(i) {
		ENUM_TO_P_IN_CASE(PPSI_EXT_NONE, char_p);
		ENUM_TO_P_IN_CASE(PPSI_EXT_WR, char_p);
		ENUM_TO_P_IN_CASE(PPSI_EXT_L1S, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_wrpc_mode_cfg:
		i = wrpc_get_i32(p);
		switch(i) {
		ENUM_TO_P_IN_CASE(WRC_MODE_UNKNOWN, char_p);
		ENUM_TO_P_IN_CASE(WRC_MODE_GM, char_p);
		ENUM_TO_P_IN_CASE(WRC_MODE_MASTER, char_p);
		ENUM_TO_P_IN_CASE(WRC_MODE_SLAVE, char_p);
		ENUM_TO_P_IN_CASE(WRC_MODE_ABSCAL, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_timing_mode:
		i = wrpc_get_i32(p);
		switch(i) {
		ENUM_TO_P_IN_CASE(WRH_TM_GRAND_MASTER, char_p);
		ENUM_TO_P_IN_CASE(WRH_TM_FREE_MASTER, char_p);
		ENUM_TO_P_IN_CASE(WRH_TM_BOUNDARY_CLOCK, char_p);
		ENUM_TO_P_IN_CASE(WRH_TM_DISABLED, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_ppi_state:
	case dump_type_ppi_state_Enumeration8:
		switch(i) {
		ENUM_TO_P_IN_CASE(PPS_END_OF_TABLE, char_p);
		ENUM_TO_P_IN_CASE(PPS_INITIALIZING, char_p);
		ENUM_TO_P_IN_CASE(PPS_FAULTY, char_p);
		ENUM_TO_P_IN_CASE(PPS_DISABLED, char_p);
		ENUM_TO_P_IN_CASE(PPS_LISTENING, char_p);
		ENUM_TO_P_IN_CASE(PPS_PRE_MASTER, char_p);
		ENUM_TO_P_IN_CASE(PPS_MASTER, char_p);
		ENUM_TO_P_IN_CASE(PPS_PASSIVE, char_p);
		ENUM_TO_P_IN_CASE(PPS_UNCALIBRATED, char_p);
		ENUM_TO_P_IN_CASE(PPS_SLAVE, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_wr_config:
	case dump_type_wr_config_Enumeration8:
		switch(i) {
		ENUM_TO_P_IN_CASE(NON_WR, char_p);
		ENUM_TO_P_IN_CASE(WR_M_ONLY, char_p);
		ENUM_TO_P_IN_CASE(WR_S_ONLY, char_p);
		ENUM_TO_P_IN_CASE(WR_M_AND_S, char_p);
		ENUM_TO_P_IN_CASE(WR_MODE_AUTO, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_wr_role:
	case dump_type_wr_role_Enumeration8:
		switch(i) {
		ENUM_TO_P_IN_CASE(WR_ROLE_NONE, char_p);
		ENUM_TO_P_IN_CASE(WR_MASTER, char_p);
		ENUM_TO_P_IN_CASE(WR_SLAVE, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_pp_pdstate:
		switch(i) {
		ENUM_TO_P_IN_CASE(PP_PDSTATE_NONE, char_p);
		ENUM_TO_P_IN_CASE(PP_PDSTATE_WAIT_MSG, char_p);
		ENUM_TO_P_IN_CASE(PP_PDSTATE_PDETECTION, char_p);
		ENUM_TO_P_IN_CASE(PP_PDSTATE_PDETECTED, char_p);
		ENUM_TO_P_IN_CASE(PP_PDSTATE_FAILURE, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_exstate:
		switch(i) {
		ENUM_TO_P_IN_CASE(PP_EXSTATE_DISABLE, char_p);
		ENUM_TO_P_IN_CASE(PP_EXSTATE_ACTIVE, char_p);
		ENUM_TO_P_IN_CASE(PP_EXSTATE_PTP, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_pp_servo_flag:
		switch(i) {
		ENUM_TO_P_IN_CASE(PP_SERVO_FLAG_VALID, char_p);
		ENUM_TO_P_IN_CASE(PP_SERVO_FLAG_WAIT_HW, char_p);
		ENUM_TO_P_IN_CASE(PP_SERVO_FLAG_VALID | PP_SERVO_FLAG_WAIT_HW, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_pp_servo_state:
		switch(i) {
		ENUM_TO_P_IN_CASE(WRH_UNINITIALIZED, char_p);
		ENUM_TO_P_IN_CASE(WRH_SYNC_TAI, char_p);
		ENUM_TO_P_IN_CASE(WRH_SYNC_NSEC, char_p);
		ENUM_TO_P_IN_CASE(WRH_SYNC_PHASE, char_p);
		ENUM_TO_P_IN_CASE(WRH_TRACK_PHASE, char_p);
		ENUM_TO_P_IN_CASE(WRH_WAIT_OFFSET_STABLE, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_wr_state:
		switch(i) {
		ENUM_TO_P_IN_CASE(WRS_IDLE, char_p);
		ENUM_TO_P_IN_CASE(WRS_PRESENT, char_p);
		ENUM_TO_P_IN_CASE(WRS_S_LOCK, char_p);
		ENUM_TO_P_IN_CASE(WRS_M_LOCK, char_p);
		ENUM_TO_P_IN_CASE(WRS_LOCKED, char_p);
		ENUM_TO_P_IN_CASE(WRS_CALIBRATION, char_p);
		ENUM_TO_P_IN_CASE(WRS_CALIBRATED, char_p);
		ENUM_TO_P_IN_CASE(WRS_RESP_CALIB_REQ, char_p);
		ENUM_TO_P_IN_CASE(WRS_WR_LINK_ON, char_p);
		ENUM_TO_P_IN_CASE(WRS_ABSCAL, char_p);
		ENUM_TO_P_IN_CASE(WRS_MAX_STATES, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_ppi_profile:
		switch(i) {
		ENUM_TO_P_IN_CASE(PPSI_PROFILE_PTP, char_p);
		ENUM_TO_P_IN_CASE(PPSI_PROFILE_WR, char_p);
		ENUM_TO_P_IN_CASE(PPSI_PROFILE_HA, char_p);
		ENUM_TO_P_IN_CASE(PPSI_PROFILE_CUSTOM, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_ppi_proto:
		switch(i) {
		ENUM_TO_P_IN_CASE(PPSI_PROTO_RAW, char_p);
		ENUM_TO_P_IN_CASE(PPSI_PROTO_UDP, char_p);
		ENUM_TO_P_IN_CASE(PPSI_PROTO_VLAN, char_p);
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;

	case dump_type_ppi_flag:
		switch(i) {
		case 0:
			char_p = "None";
			break;
		ENUM_TO_P_IN_CASE(PPI_FLAG_WAITING_FOR_F_UP, char_p);
		ENUM_TO_P_IN_CASE(PPI_FLAG_WAITING_FOR_RF_UP, char_p);
		case PPI_FLAGS_WAITING:
		    char_p = "PPI_FLAG_WAITING_FOR_F_UP | PPI_FLAG_WAITING_FOR_RF_UP";
		    break;
		default:
			char_p = "Unknown";
		}
		printf("%d", i);
		print_str(char_p);
		printf("\n");
		break;
	}
}
void dump_many_fields(void *addr, char *name, char *prefix)
{
	struct dump_info *p = dump_info;

	/* Look for name */
	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, name))
			break;

	if (!strcmp(p->name, "end")) {
		fprintf(stderr, "structure \"%s\" not described\n", name);
		return;
	}

	endian_flag = p->endian_flag;
	for (p++; p->endian_flag == 0; p++)
		dump_one_field(addr, p, prefix);
}
unsigned long wrpc_get_pointer(void *base, char *s_name, char *f_name)
{
	struct dump_info *p = dump_info;
	int offset;

	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, s_name))
			break;

	if (!strcmp(p->name, "end")) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}
	endian_flag = p->endian_flag;
	/* Look for the field: we find the offset,  */
	for (p++; p->endian_flag == 0; p++) {
		if (!strcmp(p->name, f_name)) {
			offset = wrpc_get_i32(&p->offset);
			return wrpc_get_l32(base + offset);
		}
	}
	fprintf(stderr, "can't find \"%s\" in \"%s\"\n", f_name, s_name);
	return 0;
}

/* get an offset of a field in a structure */
unsigned long wrpc_get_offset(char *s_name, char *f_name)
{
	struct dump_info *p = dump_info;
	int offset;

	for (; strcmp(p->name, "end"); p++)
		if (!strcmp(p->name, s_name))
			break;

	if (!strcmp(p->name, "end")) {
		fprintf(stderr, "structure \"%s\" not described\n", s_name);
		return 0;
	}
	endian_flag = p->endian_flag;
	/* Look for the field: we find the offset,  */
	for (p++; p->endian_flag == 0; p++) {
		if (!strcmp(p->name, f_name)) {
			offset = wrpc_get_i32(&p->offset);
			return offset;
		}
	}
	fprintf(stderr, "can't find \"%s\" in \"%s\"\n", f_name, s_name);
	return 0;
}

void print_version(void)
{
	fprintf(stderr, "Built in wrpc-sw repo ver:%s, by %s on %s %s\n",
		__GIT_VER__, __GIT_USR__, __TIME__, __DATE__);
	fprintf(stderr, "Supported WRPC structures version %d\n",
		WRPC_SHMEM_VERSION);
	fprintf(stderr, "Supported PPSI structures version %d\n",
		WRS_PPSI_SHMEM_VERSION);
}
/* all of these are 0 by default */
unsigned long spll_off, fifo_off, ppi_off, ppg_off, servo_off, ds_off,
	      stats_off;

/* Use:  wrs_dump_memory <file> <hex-offset> <name> */
int main(int argc, char **argv)
{
	int fd;
	void *mapaddr;
	unsigned long offset;
	struct stat st;
	char *dumpname = "";
	char *prefix;
	char c;
	uint8_t version_wrpc, version_ppsi;

	if (argc != 4 && argc != 2) {
		fprintf(stderr, "%s: use \"%s <file> <offset> <name>\n",
			argv[0], argv[0]);
		fprintf(stderr,
			"\"name\" is one of pll, fifo, ppg, ppi, servo_state"
			" or ds for data-sets. \"ds\" gets a ppg offset\n");
		fprintf(stderr, "But with a new binary, just pass <file>\n\n");
		print_version();
		exit(1);
	}

	fd = open(argv[1], O_RDONLY | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: %s: %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "%s: stat(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	if (!S_ISREG(st.st_mode)) { /* FIXME: support memory */
		fprintf(stderr, "%s: %s not a regular file\n",
			argv[0], argv[1]);
		exit(1);
	}

	if (st.st_size > 256 * 1024) /* support /sys/..../resource0 */
		st.st_size = 256 * 1024;

	if (argc == 4 && sscanf(argv[2], "%lx%c", &offset, &c) != 1) {
		fprintf(stderr, "%s: \"%s\" not a hex offset\n", argv[0],
			argv[2]);
		exit(1);
	}
	mapaddr = mmap(0, st.st_size, PROT_READ | PROT_WRITE,
		       MAP_FILE | MAP_PRIVATE, fd, 0);
	if (mapaddr == MAP_FAILED) {
		fprintf(stderr, "%s: mmap(%s): %s\n",
			argv[0], argv[1], strerror(errno));
		exit(1);
	}
	printf("map at 0x%p size 0x%zx\n", mapaddr, st.st_size);
	/* In case we have a "new" binary file, use such information */
	if (!strncmp(mapaddr + WRPC_MARK, "CPRW", 4))
		setenv("WRPC_SPEC", "yes", 1);

	/* If the dump file needs "spec" byte order, fix it all */
	if (getenv("WRPC_SPEC")) {
		uint32_t *p = mapaddr;
		int i;

		for (i = 0; i < st.st_size / 4; i++, p++)
			*p = __bswap_32(*p);
	}

	if (argc == 4)
		dumpname = argv[3];

	/* If we have a new binary file, pick the pointers
	 * Magic numbers are taken from crt0.S or disassembly of wrc.bin */
	if (!strncmp(mapaddr + WRPC_MARK, "WRPC----", 8)) {

		spll_off = wrpc_get_l32(mapaddr + SOFTPLL_PADDR);
		fifo_off = wrpc_get_l32(mapaddr + FIFO_LOG_PADDR);
		ppg_off = wrpc_get_l32(mapaddr + PPG_STATIC_PADDR);
		stats_off = wrpc_get_l32(mapaddr + STATS_PADDR);
		if (ppg_off) { /* This is 0 for wrs */
			ppi_off = wrpc_get_pointer(mapaddr + ppg_off,
				   "pp_globals", "pp_instances");
			ds_off = ppg_off;
		}
	}

	/* Check the version of wrpc and ppsi structures */
	version_wrpc = wrpc_get_8(mapaddr + VERSION_WRPC_ADDR);
	version_ppsi = wrpc_get_8(mapaddr + VERSION_PPSI_ADDR);
	if (version_wrpc != WRPC_SHMEM_VERSION) {
		printf("Unsupported version of WRPC structures! Expected %d, "
		       "but read %d\n", WRPC_SHMEM_VERSION, version_wrpc);
		exit(1);
	}
	if (version_ppsi != WRS_PPSI_SHMEM_VERSION) {
		printf("Unsupported version of PPSI structures! Expected %d, "
		       "but read %d\n", WRS_PPSI_SHMEM_VERSION, version_ppsi);
		exit(1);
	}

	#define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)

	/* Now check the "name" to be dumped  */
	if (!strcmp(dumpname, "pll"))
		spll_off = offset;
	if (spll_off) {
		prefix = "spll";
		printf("%s at 0x%lx\n", prefix, spll_off);
		dump_many_fields(mapaddr + spll_off, "softpll", prefix);
	}
	if (!strcmp(dumpname, "fifo"))
		fifo_off = offset;
	if (fifo_off) {
		int i;

		printf("fifo log at 0x%lx\n", fifo_off);
		for (i = 0; i < FIFO_LOG_LEN; i++)
			dump_many_fields(mapaddr + fifo_off
					 + i * sizeof(struct spll_fifo_log),
					 "pll_fifo", "fifo");
	}
	if (!strcmp(dumpname, "ppg"))
		ppg_off = offset;
	if (ppg_off) {
		unsigned long arch_data_offset;

		prefix = "ppsi.globalDS";
		printf("%s at 0x%lx\n", prefix, ppg_off);
		dump_many_fields(mapaddr + ppg_off, "pp_globals", prefix);
		
		arch_data_offset = wrpc_get_pointer(mapaddr + ppg_off,
					     "pp_globals", "arch_data");
		prefix = "ppsi.arch_data";
		printf("%s at 0x%lx\n", prefix, arch_data_offset);
		dump_many_fields(mapaddr + arch_data_offset, "wrpc_arch_data_t", prefix);
	}
	/* This "all" gets the ppg pointer. It's not really all: no pll */
	if (!strcmp(dumpname, "ds"))
		ds_off = offset;
	if (ds_off) {
		unsigned long newoffset;

		newoffset = wrpc_get_pointer(mapaddr + ds_off,
					     "pp_globals", "defaultDS");
		prefix = "ppsi.defaultDS";
		printf("%s at 0x%lx\n", prefix, newoffset);
		dump_many_fields(mapaddr + newoffset, "defaultDS_t", prefix);

		newoffset = wrpc_get_pointer(mapaddr + ds_off,
					     "pp_globals", "currentDS");
		prefix = "ppsi.currentDS";
		printf("%s at 0x%lx\n", prefix, newoffset);
		dump_many_fields(mapaddr + newoffset, "currentDS_t", prefix);

		newoffset = wrpc_get_pointer(mapaddr + ds_off,
					     "pp_globals", "parentDS");
		prefix = "ppsi.parentDS";
		printf("%s at 0x%lx\n", prefix, newoffset);
		dump_many_fields(mapaddr + newoffset, "parentDS_t", prefix);

		newoffset = wrpc_get_pointer(mapaddr + ds_off,
					     "pp_globals", "timePropertiesDS");
		prefix = "ppsi.timePropertiesDS";
		printf("%s at 0x%lx\n", prefix, newoffset);
		dump_many_fields(mapaddr + newoffset, "timePropertiesDS_t", prefix);
	}

	/* FIXME: support multiple instances */
	if (!strcmp(dumpname, "ppi"))
		ppi_off = offset;
	if (ppi_off) {
		int protocol_extension;
		unsigned long portds_off;
		unsigned long frgn_m_off;
		int frgn_rec_num;
		int frgn_m_i;
		char buff[50];
		prefix = "ppsi.inst.0";
		printf("%s at 0x%lx\n", prefix, ppi_off);
		dump_many_fields(mapaddr + ppi_off, "pp_instance", prefix);

		/* FIXME: support multiple servo */
		servo_off = wrpc_get_pointer(mapaddr + ppi_off,
			    "pp_instance", "servo");
		prefix = "ppsi.inst.0.servo";
		printf("%s at 0x%lx\n", prefix, servo_off);
		dump_many_fields(mapaddr + servo_off, "pp_servo", prefix);

		/* dump foreign masters */
		frgn_rec_num = wrpc_get_16(mapaddr + ppi_off + wrpc_get_offset("pp_instance", "frgn_rec_num"));
		frgn_m_off = ppi_off + wrpc_get_offset("pp_instance", "frgn_master");

		prefix = "ppsi.inst.0.frgn_master";
		printf("%s at 0x%lx\n", prefix, frgn_m_off);

		for (frgn_m_i = 0; frgn_m_i < frgn_rec_num && frgn_m_i < PP_NR_FOREIGN_RECORDS; frgn_m_i++) {
			snprintf(buff , sizeof(buff), "ppsi.inst.0.frgn_master.%i", frgn_m_i);
			dump_many_fields(mapaddr + frgn_m_off + frgn_m_i * sizeof(struct pp_frgn_master),
					 "pp_frgn_master", buff);
		}

		protocol_extension = wrpc_get_i32(mapaddr + ppi_off + wrpc_get_offset("pp_instance", "protocol_extension"));
#if CONFIG_HAS_EXT_WR == 1
		if ( protocol_extension == PPSI_EXT_WR) {
			unsigned long ext_data_off;
			unsigned long ext_data_servo_off;
			unsigned long ext_data_servo_ext_off;

			ext_data_off = wrpc_get_pointer(mapaddr + ppi_off,
						"pp_instance", "ext_data");
			ext_data_servo_off = wrpc_get_offset("wr_data", "servo"); /* should be 0, but check it anyway */
			ext_data_servo_ext_off = wrpc_get_offset("wr_data", "servo_ext");
			
			printf("ppsi.inst.0.ext_date at 0x%lx\n", ext_data_off);
			prefix = "ppsi.inst.0.servo.wr";
			printf("%s at 0x%lx\n", prefix, ext_data_off + ext_data_servo_off);
			dump_many_fields(mapaddr + ext_data_off + ext_data_servo_off, "wrh_servo_t", prefix);
			prefix = "ppsi.inst.0.servo_ext.wr";
			printf("%s at 0x%lx\n", prefix, ext_data_off + ext_data_servo_ext_off);
			dump_many_fields(mapaddr + ext_data_off + ext_data_servo_ext_off, "wr_servo_ext_t", prefix);
		}
#endif
		/* FIXME: support multiple servo */
		portds_off = wrpc_get_pointer(mapaddr + ppi_off,
			    "pp_instance", "portDS");
		prefix = "ppsi.inst.0.portDS";
		printf("%s at 0x%lx\n", prefix, portds_off);
		dump_many_fields(mapaddr + portds_off, "portDS_t", prefix);
#if CONFIG_HAS_EXT_WR == 1
		if ( protocol_extension == PPSI_EXT_WR) {
			unsigned long ext_dsport_off;

			ext_dsport_off = wrpc_get_pointer(mapaddr + portds_off,
						"portDS_t", "ext_dsport");
			prefix = "ppsi.inst.0.wrportDS";
			printf("%s at 0x%lx\n", prefix, ext_dsport_off);
			dump_many_fields(mapaddr + ext_dsport_off, "wr_dsport", prefix);
		}
#endif

	}


	if (!strcmp(dumpname, "stats"))
		stats_off = offset;
	if (stats_off) {
		printf("stats at 0x%lx\n", stats_off);
		dump_many_fields(mapaddr + stats_off, "stats", "stats");
	}

	exit(0);
}
