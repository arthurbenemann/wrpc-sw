/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
/* SFP Detection / managenent functions */

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "syscon.h"
#include "i2c.h"
#include "sfp.h"
#include "storage.h"

/* Calibration data (from EEPROM if available) */
int32_t sfp_alpha = 73622176; /* default values if could not read EEPROM */
int32_t sfp_deltaTx = 0;
int32_t sfp_deltaRx = 0;
int32_t sfp_in_db = 0;

char sfp_pn[SFP_PN_LEN];

int sfp_read_part_id(char *part_id);

static int sfp_present(void)
{
	return !gpio_in(GPIO_SFP_DET);
}


int sfp_match(void)
{
	struct s_sfpinfo sfp;

	sfp_pn[0] = '\0';
	if (!sfp_present()) {
		return -ENODEV;
	}
	if (sfp_read_part_id(sfp_pn)) {
		return -EIO;
	}

	strncpy(sfp.pn, sfp_pn, SFP_PN_LEN);
	if (storage_match_sfp(&sfp) == 0) {
		sfp_in_db = SFP_NOT_MATCHED;
		return -ENXIO;
	}
	sfp_deltaTx = sfp.dTx;
	sfp_deltaRx = sfp.dRx;
	sfp_alpha = sfp.alpha;
	sfp_in_db = SFP_MATCHED;
	return 0;
}

// ====================================================================
// Additional functions for KM3NeT
// ====================================================================

static void sfp_i2c_mod_write(uint8_t addr, uint8_t reg, uint8_t * data, uint8_t len)
{
    mi2c_start(WRPC_SFP_I2C);
    mi2c_put_byte(WRPC_SFP_I2C, addr);
    mi2c_put_byte(WRPC_SFP_I2C, reg);
#ifdef DEBUG_I2C    
    printf("Writing to %02x, reg=%02x:", addr, reg);
#endif
    while (len > 0) {
#ifdef DEBUG_I2C    
        printf(" %02x", *data);
#endif
        mi2c_put_byte(WRPC_SFP_I2C, *data);
        len--;
        data++;
    }
    mi2c_stop(WRPC_SFP_I2C);
#ifdef DEBUG_I2C    
    puts(", done!\n");
#endif    
}



static void sfp_i2c_mod_read(uint8_t addr, uint8_t reg, uint8_t * data, uint8_t len)
{
#ifdef DEBUG_I2C    
    printf("Reading from %02x, reg=%02x: ", addr, reg);
#endif
  
    mi2c_start(WRPC_SFP_I2C);
    mi2c_put_byte(WRPC_SFP_I2C, addr);
    // select register to read
    mi2c_put_byte(WRPC_SFP_I2C, reg);
    
    mi2c_repeat_start(WRPC_SFP_I2C);
    
    mi2c_put_byte(WRPC_SFP_I2C, addr | 1);
    
    while (len > 0) {
        
        mi2c_get_byte(WRPC_SFP_I2C, data, len == 1);
#ifdef DEBUG_I2C            
        printf(" %02x", *data);
#endif        
        len--;
        data++;
    }

    // close connection
    mi2c_stop(WRPC_SFP_I2C);
#ifdef DEBUG_I2C        
    puts(", done!\n");
#endif
}

// selects the page for the upper 128 bytes of address space A2
static void sfp_select_page(uint8_t page)
{
    
    sfp_i2c_mod_write(0xA2, 0x7F, &page, 1);
}

uint8_t sfp_a2_read_u8(uint8_t reg) 
{
    uint8_t data;
    sfp_i2c_mod_read(0xA2, reg, &data, 1);
    return data;
}


void sfp_a2_read_u16(uint8_t reg, uint16_t * value) {
    uint8_t data[2];

#ifdef SFP_TUNING_SIMULATE
    if (reg == SFP_ADC_LASER_TEMP) {
        *value = sfp_tuning_sim_get_laser_temp();  
        return;
    } 
#endif    
    
    sfp_i2c_mod_read(0xA2, reg, data, 2);
    
    *value = data[0] << 8;
    *value |= data[1];
}



void sfp_a2_write_u16(uint8_t reg, uint16_t value) {
    uint8_t data[2];
    data[0] = 0xFF & (value >> 8);
    data[1] = 0xFF & value;
    
    sfp_i2c_mod_write(0xA2, reg, data, 2);
    
}


// 


void sfp_read_temp(uint8_t * sfp_temp, uint8_t * sfp_temp_frac) {
    uint16_t temp;
    sfp_a2_read_u16(SFP_ADC_TEMPERATURE, &temp);
    *sfp_temp = (0xFF00 & temp) >> 8;
    *sfp_temp_frac = (temp & 0xFF);
}

static int sfp_read_chksum(int start, int end, int offset, int len, uint8_t *values)
{
	int i;
	uint8_t data, sum;
	mi2c_init(WRPC_SFP_I2C);

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA0);
	mi2c_put_byte(WRPC_SFP_I2C, start);
	mi2c_repeat_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);
	mi2c_stop(WRPC_SFP_I2C);

	sum = data;

	mi2c_start(WRPC_SFP_I2C);
	mi2c_put_byte(WRPC_SFP_I2C, 0xA1);
	for (i = start + 1; i < end; ++i) {
		mi2c_get_byte(WRPC_SFP_I2C, &data, 0);
		sum = (uint8_t) ((uint16_t) sum + data) & 0xff;
		if (i >= offset && i < offset + len)	//Part Number
			values[i - offset] = data;
	}
	mi2c_get_byte(WRPC_SFP_I2C, &data, 1);	//final word, checksum
	mi2c_stop(WRPC_SFP_I2C);

	if (sum == data)
		return 0;

	return -1;
}


static int sfp_read_a0_low(int offset, int len, uint8_t *values)
{
    return sfp_read_chksum(0, 63, offset, len, values);
}


static int sfp_read_a0_mid(int offset, int len, uint8_t *values)
{
    return sfp_read_chksum(64, 95, offset, len, values);
}


static int sfp_read_oui(uint32_t * oui)
{
    uint8_t oui_bytes[3];
    
    if (sfp_read_a0_low(37, 3, oui_bytes) != 0) return -1;
    *oui = ( oui_bytes[2] << 0 ) |  ( oui_bytes[1] << 8 ) | ( oui_bytes[0] << 16 );
    return 0;
}

int sfp_read_part_id(char *part_id)
{
    return sfp_read_a0_low(40, 16, (uint8_t*)part_id);
}


// ====================================================================
// Extension for wavelength tuning
// ====================================================================

#define OUI_OESOLTIONS      0x00193A
#define OUI_LUMENTUM        0x000B40
#define OUI_JDSU            0x00019C

#ifdef SFP_TUNING_SIMULATE
static uint16_t _sim_tune = 30906;

static uint16_t sfp_tuning_sim_get_laser_temp()
{
    return 25000 + (_sim_tune / 16);
}

#endif


static int _tuning_procedure = -1;

int sfp_get_tuning_procedure(void)
{
    if (_tuning_procedure != -1)
        goto end;

#ifndef SFP_TUNING_SIMULATE
    uint32_t oui;
    tsfp_init();

    if (tsfp_supported(NULL))
    {
        // old interface bw compatible tuning
        _tuning_procedure = SFP_TUNING_PROC_TSFP;
        goto end;
    }

    if (sfp_read_oui(&oui) != 0)
    {
        goto end;
    }

    switch (oui)
    {
    case OUI_OESOLTIONS:
        _tuning_procedure = SFP_TUNING_PROC_OESOLUTIONS;
        break;
    case OUI_LUMENTUM:
        _tuning_procedure = SFP_TUNING_PROC_LUMENTUM;
        break;
    case OUI_JDSU:
        _tuning_procedure = SFP_TUNING_PROC_JDSU;
        break;
    default:
        _tuning_procedure = SFP_TUNING_PROC_NONE;
        break;
    }
#else
    _tuning_procedure = SFP_TUNING_PROC_SIMULATION;
#endif

end:
    return _tuning_procedure;
}

static void sfp_do_tune_word(int32_t * tw, bool write)
{
    uint8_t tmp[4];
 
    if (_tuning_procedure == -1) sfp_get_tuning_procedure();

#ifdef SFP_TUNING_SIMULATE
    if (write) {
      _sim_tune = *tw;
    } else {
      *tw = _sim_tune;
    }
#else    
    switch (_tuning_procedure)
    {
    case SFP_TUNING_PROC_OESOLUTIONS:
    
        tmp[0] = 0x4F; tmp[1] = 0x45; tmp[2] = 0x53; tmp[3] = 0x50;
        // unlock?
        sfp_i2c_mod_write(0xA2, 0x7B, tmp, 4);
        sfp_select_page(4);
        
        if (write) 
        {
            tmp[0] = 0xFF &  (*tw >> 8);
            tmp[1] = 0xFF &  (*tw);
            sfp_i2c_mod_write(0xA2, 0x8B, tmp, 2);
        } else {
            sfp_i2c_mod_read(0xA2, 0x8B, tmp, 2);
            *tw = 0;
            *tw = (tmp[0] << 8) | tmp[1];
        }
        // lock again
        tmp[0] = 0xFF; tmp[1] = 0xFF; tmp[2] = 0xFF; tmp[3] = 0xFF;
        sfp_i2c_mod_write(0xA2, 0x7B, tmp, 4);
        break;
    case SFP_TUNING_PROC_LUMENTUM:
        sfp_select_page(2);
        
        if (write) 
        {
            tmp[0] = 0xFF &  (*tw >> 8);
            tmp[1] = 0xFF &  (*tw);
            sfp_i2c_mod_write(0xA2, 0x90, tmp, 2);
        } else {
            sfp_i2c_mod_read(0xA2, 0x90, tmp, 2);
            *tw = 0;
            *tw = (tmp[0] << 8) | tmp[1];
        }
        break;
    case SFP_TUNING_PROC_JDSU:
        sfp_select_page(2);
        
        if (write) 
        {
            tmp[0] = 0xFF &  (*tw >> 8);
            tmp[1] = 0xFF &  (*tw);
            sfp_i2c_mod_write(0xA2, 0x90, tmp, 2);
        } else {
            sfp_i2c_mod_read(0xA2, 0x90, tmp, 2);
            *tw = 0;
            *tw = (tmp[1] << 8) | tmp[0];
        }
        break;
    case SFP_TUNING_PROC_TSFP:
        if (write) 
        {
            tsfp_tune_grid(*tw);
        } else {
            tsfp_tuning_status_t sts;
            tsfp_get_status(&sts);
            tw = sts.channel;
        }
    default:
        if (!write) {
            *tw = 0x80000000;
        }
        break;
    }
#endif    
}


int32_t sfp_get_tune_word(void)
{
    int32_t tw;
    sfp_do_tune_word(&tw, false);
    return tw;
}

void sfp_set_tune_word(int32_t tw)
{
    // printf("Request to set %d as tuneword\n", tw);
    sfp_do_tune_word(&tw, true);
}


// ====================================================================
// Extension for wavelength tuning using SFF8690
// ====================================================================
#define TSFP_OPTIONS_HI_ADDR            0x65
#define TSFP_OPTIONS_HI_SUPPORTED       0x20

#define TSFP_PAGE                       0x2
#define TSFP_TDISC_ADDR                 128
#define TSFP_LFF_ADDR                   134
#define TSFP_LLF_ADDR                   138
#define TSFP_LGRID_ADDR                 140
#define TSFP_CHNO_SET                   144
#define TSFP_WL_SET                     146
#define TSFP_DITHERING_SET              151
#define TSFP_DITHERING_SET_DISABLE      0x1
#define TSFP_FREQ_ERROR                 152
#define TSFP_WL_ERROR                   154
#define TSFP_CUR_STATUS                 168
#define TSFP_LATCH_STATUS               172

static bool _tsfp_initialized = false;
static bool _tsfp_supported = false;
static tsfp_tuning_info_t _tune_info;

void tsfp_init()
{
    if (_tsfp_initialized) return;
    uint8_t t;
    _tsfp_supported = false;
    _tsfp_initialized = true;
    if (sfp_read_a0_mid(TSFP_OPTIONS_HI_ADDR, 1, &t) < 0) return;
    if (!(t & TSFP_OPTIONS_HI_SUPPORTED)) return;

    sfp_select_page(TSFP_PAGE);

    _tune_info.options = sfp_a2_read_u8(TSFP_TDISC_ADDR);
    if (_tune_info.options & TSFP_OPTIONS_TUNABLE_BY_CHANNEL)
    {
        sfp_a2_read_u16(TSFP_LFF_ADDR, &_tune_info.first_freq);
        sfp_a2_read_u16(TSFP_LLF_ADDR, &_tune_info.last_freq);
        sfp_a2_read_u16(TSFP_LGRID_ADDR, &_tune_info.grid);
    }
    else 
    {
        _tune_info.first_freq = 0;
        _tune_info.last_freq = 0;
        _tune_info.grid = 0;
    }
    _tsfp_supported = true;
}

bool tsfp_supported(tsfp_tuning_info_t * info)
{
    if (!_tsfp_supported) return false;
    if (info) *info = _tune_info;
    return true;
}

bool tsfp_tune_grid(uint16_t chno)
{
    if (!_tsfp_supported) return false;
    if (!(_tune_info.options & TSFP_OPTIONS_TUNABLE_BY_CHANNEL)) return false;
    if (chno == 0) return false;
    unsigned int max = 1 + (_tune_info.last_freq - _tune_info.first_freq) / (_tune_info.grid);
    if (chno > max) return false;
    sfp_select_page(TSFP_PAGE);
    sfp_a2_write_u16(TSFP_CHNO_SET, chno);
    return true;
}

bool tsfp_tune_wl(uint16_t wl)
{
    if (!_tsfp_supported) return false;
    if (!(_tune_info.options & TSFP_OPTIONS_TUNABLE_BY_WAVELENGTH)) return false;
    sfp_select_page(TSFP_PAGE);
    sfp_a2_write_u16(TSFP_WL_SET, wl);
    return true;
}

bool tsfp_disable_dithering(bool disable)
{
    if (!_tsfp_supported) return false;
    if (!(_tune_info.options & TSFP_OPTIONS_TX_DITHER_SUPPORT)) return false;
    sfp_select_page(TSFP_PAGE);
    sfp_a2_write_u8(TSFP_DITHERING_SET, disable ? TSFP_DITHERING_SET_DISABLE : 0);
    return true;
}

bool tsfp_get_status(tsfp_tuning_status_t * tune_status)
{
    if (!_tsfp_supported) return false;
    tsfp_tuning_status_t s;

    // cheapskate status
    sfp_select_page(TSFP_PAGE);

    s.status = sfp_a2_read_u8(TSFP_CUR_STATUS) | (sfp_a2_read_u8(TSFP_LATCH_STATUS) << 8);
    s.status |= TSFP_DITHERING_SET_DISABLE & sfp_a2_read_u8(TSFP_DITHERING_SET);
    sfp_a2_read_u16(TSFP_FREQ_ERROR, (uint16_t*)&s.freq_err);
    sfp_a2_read_u16(TSFP_WL_ERROR, (uint16_t*)&s.wl_err);
    sfp_a2_read_u16(TSFP_WL_SET, &s.wavelength);
    sfp_a2_read_u16(TSFP_CHNO_SET, &s.channel);

    *tune_status = s;
}