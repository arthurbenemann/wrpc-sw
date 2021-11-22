/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __SFP_H
#define __SFP_H

#include <stdint.h>
#include <stdbool.h>

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
#define SFP_TUNING_PROC_TSFP            0xFF


/* Reads a 16 bits word from address space A2 */
void sfp_a2_read_u16(uint8_t reg, uint16_t * value);

/* Writes a 16 bit value on address space A2 */
void sfp_a2_write_u16(uint8_t reg, uint16_t value);

/* Returns the supported tunign procedure */
int sfp_get_tuning_procedure(void);

/* Get current tune word */
int32_t sfp_get_tune_word(void);

void sfp_read_temp(uint8_t * sfp_temp, uint8_t * sfp_temp_frac);

/* Set the tune word */
void sfp_set_tune_word(int32_t tune_word);



#define TSFP_OPTIONS_TX_DITHER_SUPPORT      0x4
#define TSFP_OPTIONS_TUNABLE_BY_CHANNEL     0x2
#define TSFP_OPTIONS_TUNABLE_BY_WAVELENGTH  0x1

/**
 * @brief Tuning infomration from SFF 8690 
 * 
 * Table 4-3:
 * 
 * first_freq = LFL1 x 1e4 + LFL2 (in 0.1 GHz)
 * last_freq  = LFH1 x 1e4 + LFH2 (in 0.1 GHz)
 * grid = LGrid
 * 
 */
typedef struct 
{
    uint32_t    first_freq;     //!< First frequency in steps of 0.1 GHz
    uint32_t    last_freq;      //!< Last frequency in steps of 0.1 GHz
    int16_t     grid;           //!< Grid spacing in steps of 0.1 GHz
    uint8_t     options;        //!< Options, mask of TSFP_OPTIONS_*
} tsfp_tuning_info_t;

/**
 * @brief Initializes the tsfp. Call this before any other tsfp_* function,
 * including tsfp_supported.
 */
void tsfp_init(void);

/**
 * @brief   Returns whether or not this transceiver supportsa the SFF8690 tunable SFP standard. 
 * 
 * This function returns to FALSE to SFP modules which are tunable, but not compleient to the SFF8690 specification!
 * 
 * @param   info        When pointer is provided and this function returns true, 
 *                      it will be filled with detailed information about this module.
 * 
 * @return  true        Yes, this module is tunnable, and tsfp_* functions are supportd
 * @return  false       No, its not supported, tsfp_* functions can not be used.
 */
bool tsfp_supported(tsfp_tuning_info_t * info);

/**
 * @brief   Initiate tuning using the ITU grid.
 * 
 * @param chno          Channel number to tune to. See SFF8690 for allowed ranges
 * 
 * @return true         Operation is a success
 * @return false        opteriona failed (When not supported, or out of range)
 */
bool tsfp_tune_grid(uint16_t chno);

/**
 * @brief   Initiate tuning using the wavelength settings
 * 
 * @note    Tuning based on wavelength is discouraged, as the code can't check the validity of the provided value.
 * 
 * @param wl            Wavelength to tune to, in steps of 0.05 nm
 * 
 * @return true         Operation is a success
 * @return false        opteriona failed
 */
bool tsfp_tune_wl(uint16_t wl);

/**
 * @brief               Enables/Disables dithering, if supported
 * 
 * @param disable       When true, disable dithering, otherwise enable it.
 * 
 * @return true         Success
 * @return false        Failed
 */
bool tsfp_disable_dithering(bool disable);

#define TSFP_STATUS_DITH_DISABLED   0x0001    //!< Dither is disabled [$]
#define TSFP_STATUS_TUNING          0x0008    //!< Tuning in progressed
#define TSFP_STATUS_UNLOCKED        0x0010    //!< Wavelength is not locked
#define TSFP_STATUS_TEC_FAULT       0x0020    //!< TEC error
#define TSFP_STATUS_DITH_UNSUP      0x0400    //!< Wavelength is not locked
#define TSFP_STATUS_BAD_CHANNEL     0x0800    //!< Bad channel request

/**
 * @brief This status object returns the current module status.
 * 
 * [$] These values are read-back from the channel number/wavelength set values. 
 *     The specification does not actually say whether or not they can be read-back
 *     and thus may provide an incorrect value for some transcievers.
 */
typedef struct 
{
    int16_t     freq_err;       //!< Freqeuncy error in steps of 0.1 Ghz
    int16_t     wl_err;         //!< Wavelength error in steps of 0.005 nm
    uint16_t    channel;        //!< Current channel as reported by module in 0.1 GHz [$]
    uint16_t    wavelength;     //!< Current wavelength as reported by module in 0.1 GHz [$]
    uint16_t    status;         //!< Combination of TSFP_STATUS_*
} tsfp_tuning_status_t;

/**
 * @brief   Returns the tuning status.
 * 
 * @param   tsfp_tuning_status_t    The tuning status structure to fill.
 * 
 * @return  true    Status successfully retrieved
 * @return  false   Failed to retrieve status
 */
bool tsfp_get_status(tsfp_tuning_status_t * tune_status);

#endif
