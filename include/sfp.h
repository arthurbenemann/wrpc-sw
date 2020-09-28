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

/* Match plugged SFP with a DB entry */
int sfp_match(void);


/* Reads the part serial number of the SFP from its configuration EEPROM */
int sfp_read_part_sn(char *part_sn);


/* Various 16 bit values */
#define SFP_TEMP_HIGH_ALARM                         0
#define SFP_TEMP_LOW_ALARM                          2
#define SFP_TEMP_HIGH_WARN                          4
#define SFP_TEMP_LOW_WARN                           6
#define SFP_VOLTAGE_HIGH_ALARM                      8
#define SFP_VOLTAGE_LOW_ALARM                       10
#define SFP_VOLTAGE_HIGH_WARN                       12
#define SFP_VOLTAGE_LOW_WARN                        14
#define SFP_BIAS_HIGH_ALARM                         16
#define SFP_BIAS_LOW_ALARM                          18
#define SFP_BIAS_HIGH_WARN                          20
#define SFP_BIAS_LOW_WARN                           22
#define SFP_TX_POWER_HIGH_ALARM                     24
#define SFP_TX_POWER_LOW_ALARM                      26
#define SFP_TX_POWER_HIGH_WARN                      28
#define SFP_TX_POWER_LOW_WARN                       30
#define SFP_RX_POWER_HIGH_ALARM                     32
#define SFP_RX_POWER_LOW_ALARM                      34
#define SFP_RX_POWER_HIGH_WARN                      36
#define SFP_RX_POWER_LOW_WARN                       38
#define SFP_LASER_TEMP_HIGH_ALARM                   40
#define SFP_LASER_TEMP_LOW_ALARM                    42
#define SFP_LASER_TEMP_HIGH_WARN                    44
#define SFP_LASER_TEMP_LOW_WARN                     46
#define SFP_TEC_CURRENT_HIGH_ALARM                  48
#define SFP_TEC_CURRENT_LOW_ALARM                   50
#define SFP_TEC_CURRENT_HIGH_WARN                   52
#define SFP_TEC_CURRENT_LOW_WARN                    54
#define SFP_ADC_TEMPERATURE                         96
#define SFP_ADC_VCC                                 98
#define SFP_ADC_TX_BIAS                             100
#define SFP_ADC_TX_POWER                            102
#define SFP_ADC_RX_POWER                            104
#define SFP_ADC_LASER_TEMP                          106
#define SFP_ADC_TEC_CURRENT                         108

#define SFP_TUNING_PROC_NONE            0x00
#define SFP_TUNING_PROC_EOPTOLINK       0x01
#define SFP_TUNING_PROC_OESOLUTIONS     0x02
#define SFP_TUNING_PROC_SIMULATION      0x03
#define SFP_TUNING_PROC_LUMENTUM        0x04
#define SFP_TUNING_PROC_JDSU            0x05


/* Reads a 16 bits word from address space A2 */
void sfp_a2_read_u16(uint8_t reg, uint16_t * value);

/* Writes a 16 bit value on address space A2 */
void sfp_a2_write_u16(uint8_t reg, uint16_t value);

/* Returns the supported tunign procedure */
int sfp_get_tuning_procedure(void);

/* Get current tune word */
int32_t sfp_get_tune_word(void);

/* Set the tune word */
void sfp_set_tune_word(int32_t tune_word);

#endif
