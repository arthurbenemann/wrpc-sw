/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wrc.h>

#include "board.h"
#include "dev/clock_monitor.h"
#include "softpll_ng.h"
#include "shell.h"

#include "ertm15_rf_distr.h"

extern struct wb_clock_monitor_device ertm14_cmon;

const char* clock_names[] = { "clk_dmtd", "clk_sys", "clk_tx1", "clk_tx2", "clk_rx" };

static int selected_config = 0;

static const char *get_rf_out_state_string(int state)
{
    switch(state)
    {
        case ERTM15_RF_OUT_ON: return "ON";
        case ERTM15_RF_OUT_OFF: return "OFF";
        case ERTM15_RF_OUT_MONITOR: return "MONITOR";
    }
    return "";
}


static void dump_dds_state( const char *name, struct ertm14_dds_state *cfg ) 
{
    int i;
    pp_printf("%s DDS FTW:                0x%08x\n", name, cfg->ftw);
    pp_printf("%s DDS amplitude factor:   %d\n", name, cfg->ampl_factor);
    pp_printf("%s DDS measured power:     %d.%-02d dBm\n", name, cfg->amp_power / 1000, cfg->amp_power % 1000);
    pp_printf("%s outputs:\n", name);
    for( i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++ )
        pp_printf("- %s%d: %-08s (last measured power = %d.%-02d dBm)\n", name, i, get_rf_out_state_string( cfg->out_state[i] ),
        cfg->out_power[i] / 1000, cfg->out_power[i] % 1000
        );
}

static void dump_config( int id, struct ertm14_board_state *cfg )
{
    int i = 0;

    pp_printf("eRTM14 config %d: ", id);

    if (!cfg->valid)
    {
        pp_printf("UNUSED\n");
        return;
    }
    if (ertm14_get_current_config_id() == id )
    {
        pp_printf("ACTIVE\n");
    } else {
        pp_printf("\n");
    }

    dump_dds_state("LO", &cfg->lo);
    dump_dds_state("REF", &cfg->ref);

    pp_printf("CLKA/CLKB outputs: \n");
    for(i = 0; i <= ERTM14_CLKAB_OUT_MAX_ID; i++)
    {
        pp_printf(" - CLKA%-02d: %-20d Hz (%s) CLKB%-02d: %-20d Hz (%s)\n",
        i, cfg->clka_freq_hz[i], (cfg->clka_enable_mask & (1<<i)) ? "ON " : "OFF",
        i, cfg->clkb_freq_hz[i], (cfg->clkb_enable_mask & (1<<i)) ? "ON " : "OFF" );

    }
}

#define PARAM_FTW 0
#define PARAM_AMPL 1
#define PARAM_ENABLE 2
#define PARAM_FREQ 3

static void set_dds_param(int param, const char *name, const char *value, const char *value2)
{

    if( !name || !value )
    {
        pp_printf("Too few arguments.\n");
        return;
    }

    int is_lo = !strcasecmp( name , "lo");
    int is_ref = !strcasecmp( name , "ref");
    
    struct ertm14_board_state *cfg = ertm14_get_state_for_config(selected_config);
    cfg->valid = 1;

    if(is_lo || is_ref)
    {
        struct ertm14_dds_state *dcfg = is_lo ? &cfg->lo : &cfg->ref;

        switch(param)
        {
            case PARAM_AMPL:    dcfg->ampl_factor = strtol(value, NULL, 0); break;
            case PARAM_FTW:    dcfg->ftw = strtol(value, NULL, 0); break;
            case PARAM_ENABLE:
            {
                 int out = atoi(value);
                 if( out >= ERTM14_RF_OUT_MIN_ID && out <= ERTM14_RF_OUT_MAX_ID )
                 {
                    dcfg->out_state[out] = atoi(value2) ? ERTM15_RF_OUT_ON : ERTM15_RF_OUT_OFF;
                 } else {
                     pp_printf("Expected LO/REF output index\n");
                 }
                 break;
            }
            default: break;
        }
    } else {
        pp_printf("expected DDS name: lo ref\n");
    }
}

static void set_clk_param(int param, const char *name, const char *channel, const char *value)
{
    int is_clka = !strcasecmp( name , "clka");
    int is_clkb = !strcasecmp( name , "clkb");
    
    struct ertm14_board_state *cfg = ertm14_get_state_for_config(selected_config);
    cfg->valid = 1;

    
    if (is_clka || is_clkb)
    {
        
        uint32_t *freq = is_clka ? cfg->clka_freq_hz : cfg->clkb_freq_hz;
        uint32_t *enable_mask = is_clka ? &cfg->clka_enable_mask : &cfg->clkb_enable_mask;
        
        int ch = atoi(channel);

        switch(param)
        {
            case PARAM_FREQ:    freq[ch] = atoi(value); break;
            case PARAM_ENABLE:
            {
              if(atoi(value))
                *enable_mask |= (1<<ch);
            else
                *enable_mask &= ~(1<<ch);


            }
            default: break;
        }

    } else {
        pp_printf("expected CLK name: clka clkb\n");
    }
}

static int measure_clock(int id, int ref_channel, int ref_frequency)
{
    struct wb_clock_monitor_device *cm = &board.ertm14_cmon;

    wb_cm_set_ref_frequency(cm, ref_frequency);
    wb_cm_configure(cm, ref_channel, 2, 6250000);
    wb_cm_restart(cm);

    while (!(cm->freq_valid_mask & (1 << id)))
        wb_cm_read(cm);

    return cm->freqs[id];
}

static void ertm_test_dac()
{
    int i = 0;
    pp_printf("Playing sawtooth on eRTM15 OCXO DAC. Press ESC to abort.\n");

    for (;;)
    {
        int c = console_getc();

        if (c == 0x1b)
            break;

        spll_set_dac(0, i & 0xffff);

        i += 100;

        timer_delay(5);
    }
}

static void ertm_show_cm()
{
    int i;
    struct wb_clock_monitor_device *cm = &board.ertm14_cmon;

    wb_cm_set_ref_frequency(cm, DMTD_CLOCK_FREQ_HZ);
    wb_cm_configure(cm, ERTM14_CMON_CLK_DMTD, 2, 6250000);
    wb_cm_restart(cm);
    while (!(cm->freq_valid_mask & (1 << 0)))
        wb_cm_read(cm);
    for (i = 0; i < 5; i++)
    {
        pp_printf("cm%d: %d (valid=%d)\n", i, cm->freqs[i], cm->freq_valid_mask & (1 << i) ? 1 : 0);
    }
}

static void set_dds_sync_source( char *channel_name, char *src_name )
{

}

static int cmd_ertm(const char *args[])
{
	int i;
    if (!strcasecmp(args[0], "test-dac")) 
    {
        ertm_test_dac();
    }
    else if (!strcasecmp(args[0], "cm")) 
    {
        ertm_show_cm();
	}
    else if (!strcasecmp(args[0], "test-clocks")) 
    {
		pp_printf("eRTM14/15 clock frequency test:\n");

        phy_calibration_disable();
        spll_init( SPLL_MODE_DISABLED, 0, 0);

      /*  for(;;)
        {
        spll_set_dac(-1, 0); // dmtd -> min
        usleep(500000);
        spll_set_dac(-1, 65530); // dmtd -> max
        usleep(500000);
        pp_printf(".");
        
        }*/
      

        pp_printf("Main Ref clock: ");
        
        spll_set_dac(0, 0); // main -> min
        usleep(500000);
        int main_min = measure_clock( ERTM14_CMON_CLK_REF, ERTM14_CMON_CLK_DMTD, DMTD_CLOCK_FREQ_HZ );

        spll_set_dac(0, 65530); // main -> max
        usleep(500000);
        int main_max = measure_clock( ERTM14_CMON_CLK_REF, ERTM14_CMON_CLK_DMTD, DMTD_CLOCK_FREQ_HZ );
        
        spll_set_dac(0, 32768); // main -> midrange
        usleep(500000);
        int main_mid = measure_clock( ERTM14_CMON_CLK_REF, ERTM14_CMON_CLK_DMTD, DMTD_CLOCK_FREQ_HZ );

        pp_printf("min=%d, max=%d, mid=%d Hz\n", main_min, main_max, main_mid);
        
        pp_printf("DMTD clock: ");

        spll_set_dac(-1, 0); // dmtd -> min
        usleep(500000);
        int dmtd_min = measure_clock( ERTM14_CMON_CLK_DMTD, ERTM14_CMON_CLK_REF, 20000000 );

        spll_set_dac(-1, 65530); // dmtd -> max
        usleep(500000);
        int dmtd_max = measure_clock( ERTM14_CMON_CLK_DMTD, ERTM14_CMON_CLK_REF, 20000000 );
        
        spll_set_dac(-1, 32768); // dmtd -> midrange
        usleep(500000);
        int dmtd_mid = measure_clock( ERTM14_CMON_CLK_DMTD, ERTM14_CMON_CLK_REF, 20000000 );

        pp_printf("min=%d, max=%d, mid=%d Hz\n", dmtd_min, dmtd_max, dmtd_mid);
        


    } else if (!strcasecmp(args[0], "show-config") ) {
        dump_config( i, ertm14_get_state_for_config( selected_config ) );

    } else if (!strcasecmp(args[0], "activate-config") ) {
        if( !args[1] )
        {
            pp_printf("expected configuration ID\n");
        }
        int id = atoi(args[1]);
        pp_printf("Activating configuration %d:\n", id );
        dump_config( id, ertm14_get_state_for_config( id ) );

        ertm14_apply_config( id );


    } else if (!strcasecmp(args[0], "select-config")) {
        if(args[1])
            selected_config = atoi( args[1] );

        pp_printf("Selected configuration: %d\n", selected_config);
    } else if (!strcasecmp(args[0], "set-dds-ftw")) {
        set_dds_param(PARAM_FTW, args[1], args[2], args[3] );
    } else if (!strcasecmp(args[0], "set-dds-ampl")) {
        set_dds_param(PARAM_AMPL, args[1], args[2], 0 );
    } else if (!strcasecmp(args[0], "set-dds-enable")) {
        set_dds_param(PARAM_ENABLE, args[1], args[2], args[3]);
    } else if (!strcasecmp(args[0], "set-clk-enable")) {
        set_clk_param(PARAM_ENABLE, args[1], args[2], args[3]);
    } else if (!strcasecmp(args[0], "set-clk-freq")) {
        set_clk_param(PARAM_FREQ, args[1], args[2] ,args[3]);
    } else if (!strcasecmp(args[0], "set-dds-sync-source")) {
        set_dds_sync_source(args[1], args[2]);
    }
}

#define ERTM14_MON_REFRESH_PERIOD 1000 // ms

static timeout_t ertm14_mon_timer;

static const char *nco_sync_source_to_string(int src)
{
    switch(src)
    {
        case ERTM14_SYNC_SOURCE_NONE: return "Off";
        case ERTM14_SYNC_SOURCE_PPS: return "PPS";
        case ERTM14_SYNC_SOURCE_RF_TRIGGER: return "RF Trigger";
        default: return "?";
    }
}


static int ertm14_monitor_ui(void)
{
    if( !tmo_expired( &ertm14_mon_timer ))
        return 0;

    uint32_t ret;
    uint32_t val;

    tmo_restart( &ertm14_mon_timer );

    term_clear();

	cprintf(C_BLUE, "eRTM14/15 Board Monitor");
	cprintf(C_MAGENTA, "\nEsc = exit\n\n");

    ret = diag_read_word(8, DIAG_RO_BANK, &val);

    int id = ertm14_get_current_config_id();
    struct ertm14_board_state *st = ertm14_get_state_for_config(id);

    if(!st)
        return 0;

    cprintf(C_WHITE, "NCO Sync Status:\n");
    cprintf(C_MAGENTA, "Streamer RX Message count: ");
    cprintf(C_WHITE, "%d\n", val);
    cprintf(C_MAGENTA, "LO DDS Sync Mode:          ");
    cprintf(C_WHITE, "%s\n", nco_sync_source_to_string(st->lo.sync_source));
    cprintf(C_MAGENTA, "REF DDS Sync Mode:         ");
    cprintf(C_WHITE, "%s\n", nco_sync_source_to_string(st->ref.sync_source));
    cprintf(C_MAGENTA, "LO DDS Sync Triggers:      ");
    cprintf(C_WHITE, "%d\n", st->lo.sync_count);
    cprintf(C_MAGENTA, "REF DDS Sync Triggers:     ");
    cprintf(C_WHITE, "%d\n", st->ref.sync_count);



    return 0;
}

static int cmd_ertm_ui(const char *args[])
{
    tmo_init( &ertm14_mon_timer, ERTM14_MON_REFRESH_PERIOD );
	shell_activate_ui_command( ertm14_monitor_ui );
	return 0;
}

DEFINE_WRC_COMMAND(ertm) = {
	.name = "ertm",
	.exec = cmd_ertm,
};

DEFINE_WRC_COMMAND(ertm_ui) = {
	.name = "eu", // fixme
	.exec = cmd_ertm_ui,
};

void ertm14_shell_init()
{
}
