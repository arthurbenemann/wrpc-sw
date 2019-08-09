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

#ifndef __ERTM14_DDS_SYNC_H
#define __ERTM14_DDS_SYNC_H


#define DDS_SYNC_ENABLED 0x1
#define DDS_SYNC_NEGATIVE 0x2
#define DDS_SYNC_USE_EXT_FINE_DELAY 0x4
#define DDS_SYNC_CONTINUOUS 0x8

#define DDS_SYNC_N_CHANNELS 6

// sync unit channels
// SYNC_IN(+/-) of AD9910
#define ERTM14_DDS_SYNC_LO 0
#define ERTM14_DDS_SYNC_REF 1 // fixme: inverted cannel order in HDL

// SYNC_N inputs of the AD9520s (backplane clock distribution)
#define ERTM14_DDS_SYNC_CLKA 2
#define ERTM14_DDS_SYNC_CLKB 3

// I/O_UPDATE(+/-) of AD9910
#define ERTM14_DDS_IOUPDATE_LO 4
#define ERTM14_DDS_IOUPDATE_REF 5

struct dds_sync_unit_channel {
    uint32_t flags;
    int pps_offset_ps;
    int index;
    int delay_tap_size;
    int (*set_external_delay)( struct dds_sync_unit_channel* ch, int n_taps );
};

struct dds_sync_unit_device {
    void* base;
    struct dds_sync_unit_channel channels[DDS_SYNC_N_CHANNELS];
};

void dds_sync_unit_create( struct dds_sync_unit_device *dev, uint32_t base );
void dds_sync_unit_setup_channel ( struct dds_sync_unit_device* dev, int ch, int enable, int pps_offset_ps, int polarity, int continuous );
void dds_sync_unit_set_external_fine_delay ( struct dds_sync_unit_device* dev, int ch, int tap_size,  int (*set_external_delay)( struct dds_sync_unit_channel* ch, int ) );
void dds_sync_unit_trigger( struct dds_sync_unit_device* dev );
void dds_sync_force_pulse( struct dds_sync_unit_device* dev, int ch );
int dds_sync_unit_poll( struct dds_sync_unit_device* dev );

#endif
