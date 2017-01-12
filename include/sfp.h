/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __SFP_H
#define __SFP_H

#include <stdint.h>

#define SFP_PN_LEN 16
#define SFP_NOT_MATCHED 1
#define SFP_MATCHED 2

#define SFP_GET 0
#define SFP_ADD 1

extern char sfp_pn[SFP_PN_LEN];

extern int32_t sfp_in_db;
extern int32_t sfp_alpha;
extern int32_t sfp_deltaTx;
extern int32_t sfp_deltaRx;

/* Returns 1 if there's a SFP transceiver inserted in the socket. */
int sfp_present(void);

/* Reads SFP 0xA2 select a page and address */
//int sfp_rd_user(int *user_space, char *line_8);
//int sfp_rd_user(int *user_space);
int sfp_rd_a2(int8_t page, int8_t addr, int8_t *value);

/* Dump SFP 0xA2 memory 0x00 to 0xFF (first selecting a page for readout) */
int sfp_dump_a2(char *a2, int8_t page);

/* Write SFP user space (0xA2 page 0x00, 1 bytes, address 0x80) */
int sfp_write_user(int *user_space);

/* Reads SFP the laser waventlength (page 0xA0, 2 bytes, address 0x3D) */
int sfp_read_laser_wavelength(int *laser_wavelength);

/* Reads the part ID of the SFP from its configuration EEPROM */
int sfp_read_part_id(char *part_id);

/* Match plugged SFP with a DB entry */
int sfp_match(void);

#endif
