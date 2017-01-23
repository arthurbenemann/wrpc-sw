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

int sfp_sel_page2(void);

/* Reads SFP 0xA0 or 0xA2, select an address on a page */
int sfp_rd(uint8_t i2c_addr, uint8_t addr, uint8_t page, uint8_t *value);

/* Write SFP 0Xa0 or 0xA2, select an address on a page and write byte */
int sfp_wr(uint8_t i2c_addr, uint8_t addr, uint8_t page, uint8_t value);

/* Dump SFP 0xA2 memory 0x00 to 0xFF (first selecting a page for readout) */
int sfp_dump(char *memdump, uint8_t i2c_addr, uint8_t page);

int sfp_wr_ch(uint8_t ch);
int sfp_rd_ch(uint8_t *ch_number, int *ch_wl);
int sfp_rd_ch_stat(uint8_t *stat);

/* Reads SFP the laser waventlength (page 0xA0, 2 bytes, address 0x3D) */
int sfp_read_laser_wavelength(int *laser_wavelength);

/* Reads the part ID of the SFP from its configuration EEPROM */
int sfp_read_part_id(char *part_id);

/* Match plugged SFP with a DB entry */
int sfp_match(void);

#endif
