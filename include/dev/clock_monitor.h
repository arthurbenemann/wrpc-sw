#ifndef __CLOCK_MONITOR_H
#define __CLOCK_MONITOR_H

#include <stdint.h>

#define CM_MAX_CHANNELS 16

struct wb_clock_monitor_device
{
    uint32_t base;
    int prescaler;
    int gate_freq;
    int ref_sel;
    int n_channels;
    int ref_freq;
    uint32_t freqs[CM_MAX_CHANNELS];
    uint32_t freq_valid_mask;
};

int wb_cm_init( struct wb_clock_monitor_device *dev, uint32_t base_addr, int n_channels );
int wb_cm_restart( struct wb_clock_monitor_device *dev );
int wb_cm_configure(  struct wb_clock_monitor_device *dev, int ref_sel, int prescaler, int gate_freq );
int wb_cm_read(struct wb_clock_monitor_device *dev);
int wb_cm_show(struct wb_clock_monitor_device *dev);

#endif
