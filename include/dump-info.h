/*
 * This header defines structures for dumping other structures from
 * binary files. Every arch has a different endianness and alignment/size,
 * so we can't just use the structures from the host compiler. It used to
 * work for lm32/i386, but it fails with x86-64, so let's change attitude.
 */

#include <stdint.h>

/*
 * To ease copying from header files, allow int, char and other known types.
 * Please add more type as more structures are included here
 */
enum dump_type {
	dump_type_char, /* for zero-terminated strings */
	dump_type_bina, /* for binary stull in MAC format */
	/* normal types follow */
	dump_type_uint8_t,
	dump_type_uint32_t,
	dump_type_uint16_t,
	dump_type_int,
	dump_type_long_long,
	dump_type_unsigned_long,
	dump_type_unsigned_char,
	dump_type_unsigned_short,
	dump_type_double,
	dump_type_float,
	dump_type_pointer,
	/* and strange ones, from IEEE */
	dump_type_UInteger64,
	dump_type_Integer64,
	dump_type_UInteger32,
	dump_type_Integer32,
	dump_type_UInteger16,
	dump_type_Integer16,
	dump_type_UInteger8,
	dump_type_Integer8,
	dump_type_Enumeration8,
	dump_type_UInteger4,
	dump_type_Boolean,
	dump_type_ClockIdentity,
	dump_type_PortIdentity,
	dump_type_ClockQuality,
	dump_type_TimeInterval,
	dump_type_RelativeDifference,
	dump_type_FixedDelta,
	dump_type_dummy,
	/* and this is ours */
	dump_type_yes_no,
	dump_type_yes_no_Boolean,
	dump_type_spll_mode,
	dump_type_pp_time,
	dump_type_ip_address,
	dump_type_delay_mechanism,
	dump_type_protocol_extension,
	dump_type_wrpc_mode_cfg,
	dump_type_timing_mode,
	dump_type_ppi_state,
	dump_type_ppi_state_Enumeration8,
	dump_type_wr_config,
	dump_type_wr_config_Enumeration8,
	dump_type_wr_role,
	dump_type_wr_role_Enumeration8,
	dump_type_pp_pdstate,
	dump_type_exstate,
	dump_type_pp_servo_flag,
	dump_type_pp_servo_state,
	dump_type_wr_state,
	dump_type_ppi_profile,
	dump_type_ppi_proto,
	dump_type_ppi_flag,
};

/* because of the sizeof later on, we need these typedefs */
typedef void *         pointer;
typedef struct pp_time pp_time;
typedef long long      long_long;
typedef unsigned long  unsigned_long;
typedef unsigned char  unsigned_char;
typedef unsigned short unsigned_short;
typedef uint8_t        dummy; /* use the smallest */
typedef struct {unsigned char addr[4];} ip_address;
typedef Boolean        yes_no_Boolean;
typedef uint8_t        yes_no;
typedef int            spll_mode;
typedef int            delay_mechanism;
typedef int            protocol_extension;
typedef wrh_timing_mode_t timing_mode;
typedef int            wrpc_mode_cfg;
typedef int            ppi_state;
typedef Enumeration8   ppi_state_Enumeration8;
typedef int            wr_config;
typedef Enumeration8   wr_config_Enumeration8;
typedef int            wr_role;
typedef Enumeration8   wr_role_Enumeration8;
typedef pp_pdstate_t   pp_pdstate;
typedef pp_exstate_t   exstate;
typedef unsigned long  pp_servo_flag;
typedef int            pp_servo_state;
typedef wr_state_t     wr_state;
typedef int            ppi_profile;
typedef int            ppi_proto;
typedef unsigned char  ppi_flag;

/*
 * This is generated with the target compiler, and then linked
 * by the host compiler, so size and alignment must be safe. Then, the
 * first structure in each group has the endian flag and the structure name.
 * Following ones have zero in endian flag and field name.
 */
#define DUMP_ENDIAN_FLAG 0x12345678
struct dump_info {
	uint32_t endian_flag;
	uint32_t type;
	uint32_t offset;
	uint32_t size;
	char name[60];
};
extern struct dump_info dump_info[]; /* wrpc-sw/dump-info.c -> bina -> elf */

#define DUMP_HEADER(_struct) {			\
	.endian_flag = DUMP_ENDIAN_FLAG,	\
	.name = _struct,			\
}

/* The macros below rely on DUMP_STRUCT that must be externally defined */
#define DUMP_FIELD(_type, _fname) {		\
	.endian_flag = 0,			\
	.type = dump_type_ ## _type,		\
	.offset = offsetof(DUMP_STRUCT, _fname),\
	.size = sizeof(_type),			\
	.name = #_fname,			\
}
#define DUMP_FIELD_SIZE(_type, _fname, _size) { \
	.endian_flag = 0,			\
	.type = dump_type_ ## _type,		\
	.offset = offsetof(DUMP_STRUCT, _fname),\
	.size = _size,				\
	.name = #_fname,			\
}

