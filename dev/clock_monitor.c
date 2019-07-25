#include "board.h"
#include "dev/clock_monitor.h"

#include <hw/clock_monitor_regs.h>

int wb_cm_init( struct wb_clock_monitor_device *dev, uint32_t base_addr, int n_channels )
{
    dev->base =base_addr;
    dev->freq_valid_mask = 0;
    dev->n_channels = n_channels;
    dev->ref_freq = 125000000;
    return 0;
}

int wb_cm_restart( struct wb_clock_monitor_device *dev )
{
    uint32_t cr = readl( dev->base + CM_REG_CR );
    cr |= CM_CR_CNT_RST;
    dev->freq_valid_mask = 0;

    writel( cr, dev->base + CM_REG_CR );
    return 0;
}

int wb_cm_configure(  struct wb_clock_monitor_device *dev, int ref_sel, int prescaler, int gate_freq )
{
    dev->freq_valid_mask = 0;
    dev->ref_sel = ref_sel;
    dev->prescaler = prescaler;
    dev->gate_freq = gate_freq;

     writel( (dev->ref_sel << CM_CR_REFSEL_SHIFT)
     | (dev->prescaler << CM_CR_PRESC_SHIFT), dev->base + CM_REG_CR );
     writel( dev->gate_freq, dev->base + CM_REG_REFDR );

    return wb_cm_restart(dev);
}

int wb_cm_read(struct wb_clock_monitor_device *dev)
{
    int n_new = 0;
    int i;
    uint32_t rv;
    //pp_printf("CmRead\n");
    for(i = 0; i < dev->n_channels; i++)
    {
        writel( i, dev->base + CM_REG_CNT_SEL );
        rv = readl ( dev->base + CM_REG_CNT_VAL );
        if( rv & CM_CNT_VAL_VALID )
        {
      //      pp_printf("f%d %d\n", i, rv & 0x7fffffff);
            dev->freqs[i] = (int64_t) (rv & 0x7fffffff) * ( dev->ref_freq ) / (dev->gate_freq);
            dev->freq_valid_mask |= (1<<i);
            n_new++;
        }
        writel( CM_CNT_VAL_VALID, dev->base + CM_REG_CNT_VAL );
    }

    //pp_printf("Nn %d\n", n_new );
    return n_new;
}

int wb_cm_show(struct wb_clock_monitor_device *dev)
{
    int i;

    for( i = 0; i < dev->n_channels; i++ )
    {
        if( dev->freq_valid_mask & (1<<i)) 
        {
            pp_printf("Chan %d: %d Hz\n", i, dev->freqs[i]);
        }
    }
    return 0;
}
