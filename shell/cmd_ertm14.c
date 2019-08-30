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
#include "shell.h"

#include "dev/ertm15_rf_distr.h"

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


static void dump_dds_state( const char *name, struct ertm14_dds_config *cfg ) 
{
    int i;
    pp_printf("%s DDS frequency:          %-09d Hz\n", name, cfg->freq_hz);
    pp_printf("%s DDS amplitude factor:   %d\n", name, cfg->ampl_factor);
    pp_printf("%s DDS measured power:     %d.%-02d dBm\n", name, cfg->amp_power / 1000, cfg->amp_power % 1000);
    pp_printf("%s outputs:\n", name);
    for( i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++ )
        pp_printf("- %s%d: %-08s (last measured power = %d.%-02d dBm)\n", name, i, get_rf_out_state_string( cfg->out_state[i] ),
        cfg->out_power[i] / 1000, cfg->out_power[i] % 1000
        );
}

static void dump_config( int id, struct ertm14_board_config *cfg )
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
}

#define PARAM_FREQ 0
#define PARAM_AMPL 1

static void set_clock_param(int param, const char *name, const char *value)
{

    if( !name || !value )
    {
        pp_printf("Too few arguments.\n");
        return;
    }

    int is_lo = !strcasecmp( name , "lo");
    int is_ref = !strcasecmp( name , "ref");

    struct ertm14_board_config *cfg = ertm14_get_config(selected_config);
    cfg->valid = 1;

    if(is_lo || is_ref)
    {
        struct ertm14_dds_config *dcfg = is_lo ? &cfg->lo : &cfg->ref;

        switch(param)
        {
            case PARAM_AMPL:    dcfg->ampl_factor = atoi(value); break;
            case PARAM_FREQ:    dcfg->freq_hz = atoi(value); break;
            default: break;
        }
    }
}

static int cmd_ertm(const char *args[])
{
	int i;
    
	if (!strcasecmp(args[0], "test-clocks")) {
        struct wb_clock_monitor_device* cm = &board.ertm14_cmon;
		pp_printf("eRTM14/15 clock frequency test:\n");
        wb_cm_restart( cm );
        usleep(3000000);
        wb_cm_read( cm );

        for( i = 0; i < cm->n_channels; i++ )
        {
            if( cm->freq_valid_mask & (1<<i))
            {
                pp_printf("%d [%s] : %d Hz\n", i, clock_names[i], cm->freqs[i]);
            }
         }
    } else if (!strcasecmp(args[0], "show-config") ) {
        for(i = 0; i < ERTM14_MAX_CONFIGS; i++)
            dump_config( i, ertm14_get_config(i) );

    } else if (!strcasecmp(args[0], "select-config")) {
        if(args[1])
            selected_config = atoi( args[1] );

        pp_printf("Selected configuration: %d\n", selected_config);
    } else if (!strcasecmp(args[0], "set-freq")) {
        set_clock_param(PARAM_FREQ, args[1], args[2]);
        dump_config( selected_config, ertm14_get_config(selected_config) );

    } else if (!strcasecmp(args[0], "set-ampl")) {
        set_clock_param(PARAM_AMPL, args[1], args[2]);
        dump_config( selected_config, ertm14_get_config(selected_config) );
    }
}

DEFINE_WRC_COMMAND(ertm) = {
	.name = "ertm",
	.exec = cmd_ertm,
};
