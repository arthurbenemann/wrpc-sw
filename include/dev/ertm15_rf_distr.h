/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2019 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ERTM15_RF_DISTR_H
#define __ERTM15_RF_DISTR_H

#include <stdint.h>


#define ERTM15_RF_OUT_ON 0
#define ERTM15_RF_OUT_MONITOR 1
#define ERTM15_RF_OUT_OFF 2

#define ERTM15_RF_LO 0
#define ERTM15_RF_REF 1

struct ad7888_device;


struct ertm15_rf_distribution_device {
    uint16_t pwr_lo_valid;
    uint16_t lo_enabled;
    uint16_t pwr_ref_valid;
    uint16_t ref_enabled;
    int pwr_lo_ch [ 16 ];
    int pwr_ref_ch [ 16 ];
    int pwr_lo_in;
    int pwr_ref_in;
    struct ad7888_device *pwr_mon_adc;
};


void ertm15_rf_distr_init( struct ertm15_rf_distribution_device *dev, struct ad7888_device *pwr_mon_adc );
int ertm15_rf_distr_measure_power ( struct ertm15_rf_distribution_device *dev );
void ertm15_rf_distr_output_enable( struct ertm15_rf_distribution_device *dev, int path, int channel, int enabled );


#endif
