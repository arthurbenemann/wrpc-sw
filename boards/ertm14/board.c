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


#include <stdint.h>
#include <stdio.h>
#include <ppsi/ppsi.h>

#include "dev/gpio.h"
#include "dev/bb_spi.h"
#include "dev/ad951x.h"
#include "dev/ltc6950.h"
#include "dev/ad9910.h"
#include "dev/ad9520.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/pps_gen.h"
#include "dev/console.h"
#include "dev/endpoint.h"
#include "softpll_ng.h"
#include "storage.h"
#include "wrc_ptp.h"

#include <hw/wr_streamers.h>

#include "ertm15_rf_distr.h"
#include "rf_frame_transceiver.h"

// allows the eRTM14 board to operate *without* the eRTM15 (no WR support, useful for IPMI testing)
#undef CONFIG_ERTM14_WITHOUT_ERTM15

#include "hw/wb_10mhz_align_unit.h"
#include "wrc-task.h"

#define ERTM14_IUART_MAX_PAYLOAD 100

#define ERTM14_IUART_MSG_MMC_UPDATE 0
#define ERTM14_IUART_MSG_IPMI_CONSOLE_REQ 2
#define ERTM14_IUART_MSG_IPMI_SNMP_REQ 3
#define ERTM14_IUART_MSG_IPMI_CONSOLE_RESP 4
#define ERTM14_IUART_MSG_IPMI_SNMP_RESP 5

#define ERTM14_PSYNC_DEBUG

struct ertm14_board board;
static struct ertm14_board_state ertm14_configs[ ERTM14_MAX_CONFIGS ];
static struct ertm14_board_state *ertm14_current_state;

static struct gpio_pin pin_pll_main_cs_n = { &board.gpio_aux, 0 };
static struct gpio_pin pin_pll_main_sdi = { &board.gpio_aux, 1 };
static struct gpio_pin pin_pll_main_sdo = { &board.gpio_aux, 2 };
static struct gpio_pin pin_pll_main_sclk = { &board.gpio_aux, 3 };
static struct gpio_pin pin_pll_main_reset = { &board.gpio_aux, 4 };
static struct gpio_pin pin_pll_main_lock = { &board.gpio_aux, 5 };

static struct gpio_pin pin_pll_ext_cs_n = { &board.gpio_aux, 6 };
static struct gpio_pin pin_pll_ext_sdi = { &board.gpio_aux, 7 };
static struct gpio_pin pin_pll_ext_sdo = { &board.gpio_aux, 8 };
static struct gpio_pin pin_pll_ext_sclk = { &board.gpio_aux, 9 };
static struct gpio_pin pin_pll_ext_reset = { &board.gpio_aux, 10 };
static struct gpio_pin pin_pll_ext_lock = { &board.gpio_aux, 11 };

static struct gpio_pin pin_mac_addr_scl = { &board.gpio_aux, 13 };
static struct gpio_pin pin_mac_addr_sda = { &board.gpio_aux, 12 };

static struct gpio_pin pin_main_xo_en_n = { &board.gpio_aux, 14 };

static struct gpio_pin pin_ltc6950_sclk = { &board.gpio_aux, 15 };
static struct gpio_pin pin_ltc6950_sdi = { &board.gpio_aux, 16 };
static struct gpio_pin pin_ltc6950_sdo = { &board.gpio_aux, 17 };
static struct gpio_pin pin_ltc6950_ce_gen = { &board.gpio_aux, 18 };
static struct gpio_pin pin_ltc6950_ce_distr = { &board.gpio_aux, 19 };
static struct gpio_pin pin_ltc6950_sync = { &board.gpio_aux, 20 };

static struct gpio_pin pin_ad9910_lo_sdio = { &board.gpio_aux, 4+21 };
static struct gpio_pin pin_ad9910_lo_sclk = { &board.gpio_aux, 5+21 };
static struct gpio_pin pin_ad9910_lo_reset = { &board.gpio_aux, 6+21 };
static struct gpio_pin pin_ad9910_lo_io_update = { &board.gpio_aux, 3+21 };
const struct gpio_pin pin_ad9910_lo_sync_smp_err = { &board.gpio_aux, 5+21 };

static struct gpio_pin pin_ad9910_ref_sdio = { &board.gpio_aux, 4+28 };
static struct gpio_pin pin_ad9910_ref_sclk = { &board.gpio_aux, 5+28 };
static struct gpio_pin pin_ad9910_ref_reset = { &board.gpio_aux, 6+28 };
static struct gpio_pin pin_ad9910_ref_io_update = { &board.gpio_aux, 3+28 };
const struct gpio_pin pin_ad9910_ref_sync_smp_err = { &board.gpio_aux, 5+28 };

static struct gpio_pin pin_ocxo_override = { &board.gpio_aux, 48 };
static struct gpio_pin pin_ocxo_cs_n = { &board.gpio_aux, 51 };
static struct gpio_pin pin_ocxo_sclk = { &board.gpio_aux, 50 };
static struct gpio_pin pin_ocxo_data = { &board.gpio_aux, 49 };

static struct gpio_pin pin_pwrmon_adc_cs_n = {  &board.gpio_aux, 52 };
static struct gpio_pin pin_pwrmon_adc_dout = {  &board.gpio_aux, 46 };
static struct gpio_pin pin_pwrmon_adc_din = {  &board.gpio_aux, 47 };
static struct gpio_pin pin_pwrmon_adc_sclk = {  &board.gpio_aux, 45 };

static struct gpio_pin pin_ad9520_clka_scl = {  &board.gpio_aux, 57 };
static struct gpio_pin pin_ad9520_clka_sda = {  &board.gpio_aux, 58 };
static struct gpio_pin pin_ad9520_clkb_scl = {  &board.gpio_aux, 59 };
static struct gpio_pin pin_ad9520_clkb_sda = {  &board.gpio_aux, 60 };

static struct gpio_pin pin_sys_clk_sel_stb = {  &board.gpio_aux, 61 };
static struct gpio_pin pin_sys_clk_sel_next = {  &board.gpio_aux, 62 };

static struct ad95xx_config pll_ext_10mhz_config = 
#include "configs/ertm_14_pll_ext_10mhz.h"

static struct ad95xx_config pll_main_dot050_config =
#include "configs/ertm_14_pll_main_dot050_config.h"

static struct ad95xx_config pll_main_ocxo_config =
#include "configs/ertm_14_pll_ocxo_config.h"

static struct ltc6950_config pll_ertm15_bootstrap_config =
#include "configs/ertm_15_ltc6950_config.h"

static struct ad95xx_config clk_dist_ertm15_default_config =
#include "configs/ertm_15_ad9520_default_config.h"

static spll_gain_schedule_t spll_main_ocxo_gain_sched;

static int has_new_config = 0;
static int ertm_init_complete = 0;

static int ertm14_update_config_task(void);


// fixme: use PRESENCE_A/B pins instead of LTC6950 PLL chip
static int check_ertm15_presence(void)
{
    ltc6950_init(&board.ltc6950_pll, &board.spi_ltc6950);

    int id = ltc6950_read( &board.ltc6950_pll, 0x16 );
    
    if( id != 0x65 )
        return 0;

    return 1;
}


static void ertm14_spll_setup(void)
{
/* configure a suitable PI gain schedule for the SoftPLL: */
    spll_gain_schedule_t* gs=  &spll_main_ocxo_gain_sched;

    gs->n_stages = 2;

/* we start with ~100 Hz bandwidth to make it lock reasonably fast */
    gs->stages[0].kp = -4000 * 16;
    gs->stages[0].ki = -5 * 16;
    gs->stages[0].lock_samples = 30000;
    gs->stages[0].shift = 16;

/* once it's locked, the loop bandwidth is switched to ~0.1 Hz to filter out WR link added phase noise */
    gs->stages[1].kp = -3000;
    gs->stages[1].ki = -5;
    gs->stages[1].lock_samples = 10000;
    gs->stages[1].shift = 16;

    // disable 2nd stage for DOT050 and Morion OCXO
    if ( board.mode & ERTM14_MODE_WITHOUT_ERTM15 )
        gs->n_stages = 1;
    
    if ( board.mode & ERTM14_MODE_OCXO_10MHZ )
        gs->n_stages = 1;
        
#if 0
    gs->n_stages = 1;

/* we start with ~100 Hz bandwidth to make it lock reasonably fast */
    gs->stages[0].kp = -4000;
    gs->stages[0].ki = -5;
    gs->stages[0].lock_samples = 10000;
    gs->stages[0].shift = 12;
#endif

	spll_set_gain_schedule( gs );
}


static int ad9910_set_fine_delay( struct fine_pulse_gen_channel *ch, int n_taps )
{
    struct ad9910_device *dev;

    if(ch->index == ERTM14_DDS_SYNC_REF)
        dev = &board.dds_ad9910_ref;
    else
        dev = &board.dds_ad9910_lo;

    ad9910_configure_sync( dev, 1, n_taps );
    return 0;
}

static void ertm14_dds_trigger_ioupdate( struct ad9910_device *dev )
{
    int channel = (dev == &board.dds_ad9910_ref ? ERTM14_DDS_IOUPDATE_REF : ERTM14_DDS_IOUPDATE_LO);
    fine_pulse_gen_force_pulse( &board.dds_sync_dev, channel );
}

static int ertm14_switch_sys_clock( int use_sys_from_pll )
{
    gen_gpio_out( &pin_sys_clk_sel_next, use_sys_from_pll );
    gen_gpio_out( &pin_sys_clk_sel_stb, 1);
    gen_gpio_out( &pin_sys_clk_sel_stb, 0);
    return 0;
}

uint32_t ch_delays[] = { 100000, 100000, 100000, 100000, 100500, 100000 };
    
static int ertm14_dds_sync_init(void)
{
    const int n_params = 4;
    struct {
        uint32_t id;
        int channel;
        const char *name;
    } params[] = {
        { CAL_PARAM_DDS_LO_IOUPDATE_DELAY_PS, ERTM14_DDS_IOUPDATE_LO, "DDS LO IoUpdate" },
        { CAL_PARAM_DDS_REF_IOUPDATE_DELAY_PS, ERTM14_DDS_IOUPDATE_REF, "DDS REF IoUpdate" },
        { CAL_PARAM_CLKA_SYNC_DELAY_PS, ERTM14_PLL_SYNC_CLKA, "CLKA Dist SYNC" },
        { CAL_PARAM_CLKB_SYNC_DELAY_PS, ERTM14_PLL_SYNC_CLKB, "CLKB Dist SYNC" }
    };

    int i;

    // retrieve calibration delays on DDS IOUPDATE and CLKAB SYNC lines from the calibration stored in eeprom
    for( i = 0; i < n_params; i++ )
    {
        uint32_t val = ch_delays[ params[i].channel ];
        int res = storage_get_calibration_parameter( params[i].id, &val );
        pp_printf("Sync Unit channel '%s': delay = %d ps", params[i].name, val);

        if (res < 0)
        {
            pp_printf("(default value)");
        }

        pp_printf("\n");
    }

    fine_pulse_gen_create( &board.dds_sync_dev, BASE_ERTM14_DDS_SYNC_UNIT );

// Sync_in: continuous waveform, use external delay line (inside AD9910)
    
    // produce a continuos sync clock for the DDSes
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 1, ch_delays[ERTM14_DDS_SYNC_LO], FINE_PULSE_GEN_CONTINUOUS );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 1, ch_delays[ERTM14_DDS_SYNC_REF], FINE_PULSE_GEN_CONTINUOUS );
    fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 75, ad9910_set_fine_delay );
    fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 75, ad9910_set_fine_delay );

// DDS IOupdate: internal delay line, single-shot mode, positive polarity
    board_dbg("ref delay = %d lo delay = %d\n", ch_delays[ERTM14_DDS_IOUPDATE_REF], ch_delays[ERTM14_DDS_IOUPDATE_LO] );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_LO, 1, ch_delays[ERTM14_DDS_IOUPDATE_LO], 0);
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_REF, 1, ch_delays[ERTM14_DDS_IOUPDATE_REF], 0 );

// CLKAB Sync: internal delay line, single-shot mode, negative polarity
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKA, 1, ch_delays[ERTM14_PLL_SYNC_CLKA], FINE_PULSE_GEN_NEGATIVE );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKB, 1, ch_delays[ERTM14_PLL_SYNC_CLKB], FINE_PULSE_GEN_NEGATIVE );
    
    return 0;
}

static void ertm14_dds_sync_calibrate(void)
{
    shw_pps_gen_init();
    shw_pps_gen_enable_output(1);
    shw_pps_gen_unmask_output(1);

    int i = 0, j;

    uint32_t channel_mask = ( 1 << ERTM14_DDS_SYNC_LO ) | ( 1<< ERTM14_DDS_SYNC_REF );

    int fine = 0;

    #define MIN_SAMPLE_WINDOW_LENGTH 3
    #define AD9910_FINE_DELAY_STEP_PS 75

    struct dds_sync_window {
        int smp_err;
        int start;
        int length;
        int best_start;
        int best_length;
        int setpoint;
    } windows[2];

    for( j=0; j<2; j++ )
    {
        windows[j].best_start = -1;
        windows[j].best_length = -1;
        windows[j].start = -1;
        windows[j].length = 0;
    }

    for(i = 0; i < 100; i++)
    {
//        pp_printf("Sync [fine %d]! ", fine);

        fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 1, 100000 + fine, FINE_PULSE_GEN_CONTINUOUS );
        fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 1, 100000 + fine, FINE_PULSE_GEN_CONTINUOUS );
        fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 75, ad9910_set_fine_delay );
        fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 75, ad9910_set_fine_delay );

        fine_pulse_gen_trigger( &board.dds_sync_dev, channel_mask, 1 );
        while ( !fine_pulse_gen_is_triggered( &board.dds_sync_dev, channel_mask ) );

        windows[0].smp_err = !!gen_gpio_in( &pin_ad9910_lo_sync_smp_err );
        windows[1].smp_err = !!gen_gpio_in( &pin_ad9910_ref_sync_smp_err );
        for( j=0; j<2;j++ )
        {
            if (windows[j].smp_err)
            {

                windows[j].start = -1;
                windows[j].length = 0;
            }
            else
            {
                if( windows[j].start < 0 )
                    windows[j].start = fine;

                windows[j].length++;

                if( windows[j].length >= MIN_SAMPLE_WINDOW_LENGTH && windows[j].best_length < 0)
                {
                    windows[j].best_start = windows[j].start;
                    windows[j].best_length = windows[j].length;

                }
            }
        }

//        pp_printf("SmpERR LO %d REF %d\n", windows[0].smp_err, windows[1].smp_err);

        fine += AD9910_FINE_DELAY_STEP_PS;
    }

    for( j=0; j<2; j++ )
    {
        // sync_in fine delay setpoint is the 
        windows[j].setpoint = windows[j].best_start + ( AD9910_FINE_DELAY_STEP_PS * windows[j].best_length ) / 2;
    }
    

    pp_printf("DDS_LO SYNC start=%d ps length=%d ps setpoint=%d ps\n",
        windows[0].best_start, windows[0].best_length, windows[0].setpoint
    );
    pp_printf("DDS_REF SYNC start=%d ps length=%d ps setpoint=%d ps\n",
        windows[1].best_start, windows[1].best_length, windows[1].setpoint
    );

    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 1, 100000 + windows[0].setpoint, FINE_PULSE_GEN_CONTINUOUS );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 1, 100000 + windows[1].setpoint, FINE_PULSE_GEN_CONTINUOUS );

    
        
}

static int ertm14_align_clocks(void)
{
    uint32_t channel_mask = ( 1 << ERTM14_DDS_SYNC_LO ) | ( 1<< ERTM14_DDS_SYNC_REF );

    fine_pulse_gen_trigger( &board.dds_sync_dev, channel_mask, 1 );
    while ( !fine_pulse_gen_is_triggered( &board.dds_sync_dev, channel_mask ) );

    int smp_err_lo = !!gen_gpio_in( &pin_ad9910_lo_sync_smp_err );
    int smp_err_ref = !!gen_gpio_in( &pin_ad9910_ref_sync_smp_err );

    board_dbg( "DDS SYNC complete: errLO=%d errREF=%d\n", smp_err_lo, smp_err_ref);
    
// now that we are done syncing DDS internal clocks, disable the fpga SYNC_CLK output
    ad9910_configure_sync( &board.dds_ad9910_ref, 0, 0 );
    ad9910_configure_sync( &board.dds_ad9910_lo, 0, 0 );

    channel_mask = (1 << ERTM14_PLL_SYNC_CLKA) |
                   (1 << ERTM14_PLL_SYNC_CLKB);

    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_LO, 1, ch_delays[ERTM14_DDS_IOUPDATE_LO], 0 );

    //ch_delays[ERTM14_DDS_IOUPDATE_LO] += 200;

    fine_pulse_gen_trigger( &board.dds_sync_dev, channel_mask, 0 ); // trigger on PPS now
    while ( !fine_pulse_gen_is_triggered( &board.dds_sync_dev, channel_mask ) );

    board_dbg( "CLKAB sync complete\n");

    return 0;
}


extern struct console_device console_ipmi_dev;

static void handle_iuart_request( uint8_t *buf, int size )
{
    uint8_t tx_buf[ERTM14_IUART_MAX_PAYLOAD];
    int n_tx;
    int type = buf[0];

    if( size <= 0 )
        return;

    switch( type )
    {
        case ERTM14_IUART_MSG_IPMI_CONSOLE_REQ:
            n_tx = console_ipmi_process_request( &console_ipmi_dev, buf + 1, size - 1, tx_buf + 1, sizeof(tx_buf) - 1 );

            tx_buf[0] = ERTM14_IUART_MSG_IPMI_CONSOLE_RESP;
            iuart_send_message(&board.iuart_14, tx_buf, n_tx + 1);

            break;

        case ERTM14_IUART_MSG_IPMI_SNMP_REQ:
            //snmp_respond(uint8_t *buf);

            break;


        default:
            return;
    }
}

/* Task polls requests coming from the eRTM14 IUART from the MMC and dispatches them to handlers */
static void iuart_14_poll(void)
{
    int msg = iuart_recv_message(&board.iuart_14);

    if (msg <= 0)
        return;

    if( msg == START_INSN_CHAR_VAL )
    {
        pp_printf("req %d %d %d\n",board.iuart_14.rx_buf, board.iuart_14.rx_csize, board.iuart_14.rx_csize );
        handle_iuart_request( board.iuart_14.rx_buf, board.iuart_14.rx_csize );
    }
}

static void ertm14_clock_monitor_init(void)
{
    wb_cm_init(&board.ertm14_cmon, BASE_CLOCK_MONITOR, 5);
    wb_cm_set_ref_frequency( &board.ertm14_cmon, DMTD_CLOCK_FREQ_HZ );

    /* use the DDMTD clock as the reference frequency (we don't care much about accuracy here)
       as it's always available regardless of the configuration of the I2C/SPI chips. Prescaler of 2
       and gate freq of 6.25 MHz give fast enough measurements with sufficient digits. */
    wb_cm_configure(&board.ertm14_cmon, ERTM14_CMON_CLK_DMTD, 2, 6250000 );
}

static void ertm14_align_ref_out_to_pps(void)
{
    int i;

    shw_pps_gen_init();

    shw_pps_gen_enable_output(1);
    shw_pps_gen_unmask_output(1);

    for(i=0;i<10;)
    {
        writel( TAU_CSR_TRIG, TAU_REG_CSR + BASE_ERTM14_10MHZ_ALIGN_UNIT );
        uint32_t csr = readl( TAU_REG_CSR + BASE_ERTM14_10MHZ_ALIGN_UNIT);
        pp_printf("csr %x tau %x\n", csr, TAU_REG_CSR + BASE_ERTM14_10MHZ_ALIGN_UNIT );
        if (csr & TAU_CSR_DONE)
        {
            int val = TAU_CSR_OFFSET_R( csr );
            pp_printf("measured pps offset: %d\n", val);
            i++;
        }
        usleep(200000);
    }
}



#define CLK_PPS_STATE_START 0
#define CLK_PPS_STATE_MASTER 1
#define CLK_PPS_STATE_WAIT_SERVO 2
#define CLK_PPS_STATE_SLAVE 3
#define CLK_PPS_STATE_DONE 4
#define CLK_PPS_STATE_RUN_SYNC 5

struct ertm14_clk_pps_sync_fsm {
    int state;
    int prev_mode;
    int prev_locked;
} clk_pps_sync_state;


static void ertm14_clk_pps_sync_restart(void)
{
    struct ertm14_clk_pps_sync_fsm* fsm = &clk_pps_sync_state;

    fsm->state = CLK_PPS_STATE_START;
}

static void ertm14_clk_pps_sync_init(void)
{
    ertm14_clk_pps_sync_restart();
}

static void ertm14_clk_pps_sync_task(void)
{
    extern struct pp_instance ppi_static;
    struct pp_instance *ppi = &ppi_static;

    struct ertm14_clk_pps_sync_fsm* fsm = &clk_pps_sync_state;

    int mode = wrc_ptp_get_mode();

    if( mode != fsm->prev_mode )
    {
        board_dbg("mode changed, restarting sync fsm\n");
        fsm->state = CLK_PPS_STATE_START;
    }

    fsm->prev_mode = mode;

    switch(fsm->state)
    {
        case CLK_PPS_STATE_START:
        {
            fsm->prev_locked = -1;
        
            int mode = wrc_ptp_get_mode();

            if( mode == WRC_MODE_MASTER || mode == WRC_MODE_GM )
            {
                fsm->state = CLK_PPS_STATE_MASTER;
                board_dbg("master mode\n");
            } else {
                fsm->state = CLK_PPS_STATE_SLAVE;
                board_dbg("slave mode\n");
            }
        }
        break;

        case CLK_PPS_STATE_MASTER:
        {
            if( spll_check_lock(0) )
            {
                board_dbg("pll locked\n");
                fsm->state = CLK_PPS_STATE_RUN_SYNC;
            }
        }
        break;

        case CLK_PPS_STATE_SLAVE:
        {
            struct wr_servo_state *ss = &((struct wr_data *)ppi->ext_data)->servo_state;

            if( ss->state == WR_TRACK_PHASE )
            {
                board_dbg("servo tracking phase, proceeding with sync\n");
                fsm->state = CLK_PPS_STATE_RUN_SYNC;
            }
        }
        break;


        case CLK_PPS_STATE_RUN_SYNC:
        {
            ertm14_align_clocks();
            fsm->state = CLK_PPS_STATE_DONE;
        }
        break;

        default : break;
    }
    
}



// initializes the eRTM15 LTC6950 PLL & OCXO
void ertm15_pll_init(void)
{
    ltc6950_init(&board.ltc6950_pll, &board.spi_ltc6950);

    int id = ltc6950_read( &board.ltc6950_pll, 0x16 );
    
    if( id != 0x65 )
    {
        pp_printf("Error initializing LTC6950 (read RevID: 0x%x, expected: 0x%x)\n", id, 0x65 );
    }

    // load default 'bootstrap' config and check what is the OCXO frequency
    ltc6950_configure(&board.ltc6950_pll, &pll_ertm15_bootstrap_config);

    board_dbg("Probing OCXO frequency...");

    // measure the OCXO freq
    wb_cm_restart(&board.ertm14_cmon);
    while( ( wb_cm_read( &board.ertm14_cmon ) & ( 1 << ERTM14_CMON_CLK_PLL_FB) ) == 0 )
    {
        pp_printf(".");
        usleep(200000);
    }

    int ocxo_freq = board.ertm14_cmon.freqs[ERTM14_CMON_CLK_PLL_FB];
    
    pp_printf("%d MHz measured.\n", ocxo_freq);

    int ocxo_10mhz = ocxo_freq > ( 10000000 - 20000 ) && ocxo_freq <  ( 10000000 + 20000 );
    int ocxo_100mhz = ocxo_freq > ( 100000000 - 20000 ) && ocxo_freq <  ( 100000000 + 20000 );


    if( ! (ocxo_100mhz || ocxo_10mhz) )
    {
        pp_printf("Error: the OCXO has neither 10 nor 100 MHz center frequency. WTF?\n");
    }


    if( ocxo_10mhz )
    { // PLL: R div = 1, N div = 100, LV/CM div: 50 (20 MHz output)
        
        ltc6950_write( &board.ltc6950_pll, 0x15, 50 ); // RDIVOUT = 0, output div = 50
        board.mode |= ERTM14_MODE_OCXO_10MHZ;    
    } else if (ocxo_100mhz)
    {
        pp_printf("Using 100 mhz ocxo\n");
        //ltc6950_write( &board.ltc6950_pll, 0x15, 4 ); // RDIVOUT = 0, output div = 50
        ltc6950_write( &board.ltc6950_pll, 0x8, 0x1 ); // reference divider = 1 

        ltc6950_write( &board.ltc6950_pll, 0x15, 50 ); // RDIVOUT = 0, output div = 50
        ltc6950_write( &board.ltc6950_pll, 0x0a, 10 ); // N divider = 10 (VCO @ 1GHz, PFD @ 10 MHz)
        board.mode |= ERTM14_MODE_OCXO_100MHZ;
    }
 
    //ertm14_align_ref_out_to_pps();
}

int ertm14_init_clkab_distribution()
{
    bb_i2c_create( &board.i2c_clka_distr, &pin_ad9520_clka_scl, &pin_ad9520_clka_sda );
    bb_i2c_create( &board.i2c_clkb_distr, &pin_ad9520_clkb_scl, &pin_ad9520_clkb_sda );
    bb_i2c_init( &board.i2c_clka_distr );
    bb_i2c_init( &board.i2c_clkb_distr );

    ad9520_init( &board.dev_clka_distr, &board.i2c_clka_distr, 0x5c );
    ad9520_init( &board.dev_clkb_distr, &board.i2c_clkb_distr, 0x5c );

    pp_printf("Init CLKAB distribution\n");
    ad9520_configure( &board.dev_clka_distr, &clk_dist_ertm15_default_config);
    ad9520_configure( &board.dev_clkb_distr, &clk_dist_ertm15_default_config);

// set 250 MHz output on CLKA/CLKB on the front panel
    ad9520_set_output_divider( &board.dev_clka_distr, ERTM14_CLKAB_OUT_FRONT_PANEL, 4 ); // divide by 4 -> 250 MHz
    ad9520_set_output_divider( &board.dev_clkb_distr, ERTM14_CLKAB_OUT_FRONT_PANEL, 4 );

    ad9520_enable_output( &board.dev_clka_distr, ERTM14_CLKAB_OUT_FRONT_PANEL, 1 );
    ad9520_enable_output( &board.dev_clkb_distr, ERTM14_CLKAB_OUT_FRONT_PANEL, 1 );

// force a SYNC pulse to make sure the SYNC_N pins of the AD9520s are high
// (so that any clock output is possible)    
    fine_pulse_gen_force_pulse( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKA );
    fine_pulse_gen_force_pulse( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKB );
}

int ertm14_init_ref_clock_distribution(void)
{
    int main_stat = ad951x_init(&board.ad9516_main, &board.spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock);
    int ext_stat = ad951x_init(&board.ad9516_ext, &board.spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock);

    if( main_stat < 0 )
    {
        pp_printf( "Failed to configure the main clock distribution AD9516 (chip not responding)\n ");
        return -1;
    }

    if( ext_stat < 0 )
    {
        pp_printf( "Failed to configure the external 10 MHz clock multiplier AD9516 (chip not responding)\n ");
        return -1;
    }

    if (board.mode & ERTM14_MODE_WITHOUT_ERTM15)
    {
        gen_gpio_out(&pin_main_xo_en_n, 0); // enable DOT050 VCXO
        ad951x_configure(&board.ad9516_main, &pll_main_dot050_config);
    }
    else
    {
        gen_gpio_out(&pin_main_xo_en_n, 1); // disable DOT050 VCXO
        ad951x_configure(&board.ad9516_main, &pll_main_ocxo_config);
        // ad951x_configure(&board.ad9516_ext, &pll_ext_10mhz_config);
    }
}

int ertm15_init_dds(void)
{
// reset both DDS chips
    gen_gpio_out(&pin_ad9910_ref_reset, 1);
    gen_gpio_out(&pin_ad9910_lo_reset, 1);

    usleep(10);

    gen_gpio_out(&pin_ad9910_ref_reset, 0);
    gen_gpio_out(&pin_ad9910_lo_reset, 0);

// initialize DDS synchronizer unit
    ertm14_dds_sync_init();

    ad9910_probe( &board.dds_ad9910_ref, &board.spi_ad9910_ref, ertm14_dds_trigger_ioupdate );
    ad9910_probe( &board.dds_ad9910_lo, &board.spi_ad9910_lo, ertm14_dds_trigger_ioupdate );
}

int ertm14_init_mac_eeprom(void)
{
    bb_i2c_create( &board.i2c_mac_addr, &pin_mac_addr_scl, &pin_mac_addr_sda );
    bb_i2c_init( &board.i2c_mac_addr );

    m24aa025_init( &board.m24_mac_ids[0], &board.i2c_mac_addr, 0x50 );
    m24aa025_init( &board.m24_mac_ids[1], &board.i2c_mac_addr, 0x51 );

    uint8_t mac[6];

    m24aa025_read_mac( &board.m24_mac_ids[0], mac );
    ep_set_mac_addr( mac );
}

int ertm14_init(void)
{
    int i;
    uint32_t id;

    has_new_config = 0;
    ertm_init_complete = 0;

    memset( &board, 0, sizeof( struct ertm14_board ));


    /* eRTM14 can work independently of eRTM15. If CONFIG_ERTM14_WITHOUT_ERTM15 is set,
       the software will assume we don't have an eRTM15 sandwiched even if we do. */
#ifdef CONFIG_ERTM14_WITHOUT_ERTM15
    board.mode |= ERTM14_MODE_WITHOUT_ERTM15;
#endif


    /* apply a default, sane configuration (initialize the config struct) */
    ertm14_config_init();

    /* most of the I/Os of the slow peripherals (i2c, spi) are bitbanged. First, let's
       initialize the GPIO controller they're connected to */
    wb_gpio_create( &board.gpio_aux, BASE_AUXWB );

    /* enable the main VCXO */
    gen_gpio_set_dir(&pin_main_xo_en_n, 1);
    gen_gpio_out(&pin_main_xo_en_n, 0);


    /* initialize the SPI bus for the main PLL (IC?) */
    bb_spi_create ( &board.spi_pll_main,
        &pin_pll_main_cs_n,
        &pin_pll_main_sdi,
        &pin_pll_main_sdo,
        &pin_pll_main_sclk,
        AD951X_BIT_DELAY
        );

    /* initialize the SPI bus for the external clock (10 MHz input) PLL (IC?) */
    bb_spi_create ( &board.spi_pll_ext,
        &pin_pll_ext_cs_n,
        &pin_pll_ext_sdi,
        &pin_pll_ext_sdo,
        &pin_pll_ext_sclk,
        AD951X_BIT_DELAY
        );

    /* initialize the SPI bus for the eRTM15 PLL (IC?) */
    bb_spi_create( &board.spi_ltc6950,
        &pin_ltc6950_ce_gen,
        &pin_ltc6950_sdi,
        &pin_ltc6950_sdo,
        &pin_ltc6950_sclk,
        100 );

    /* initialize the SPI bus for the eRTM15 REF DDS (IC?) */
    bb_spi_create( &board.spi_ad9910_ref,
        NULL,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sdio,
        &pin_ad9910_ref_sclk,
        100 );

    /* initialize the SPI bus for the eRTM15 LO DDS (IC?) */
    bb_spi_create( &board.spi_ad9910_lo,
        NULL,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sdio,
        &pin_ad9910_lo_sclk,
        100 );


    /* detect if the eRTM15 is present and decide how to configure the board */
    int ertm15_present = check_ertm15_presence();

    if( !ertm15_present )
        board.mode |= ERTM14_MODE_WITHOUT_ERTM15;

    if ( board.mode & ERTM14_MODE_WITHOUT_ERTM15 )
        board_dbg( "Configuring board *WITHOUT* eRTM15 support (eRTM15 not found or disabled in software).\n");
    else
        board_dbg( "Configuring board WITH eRTM15 support.\n");
    

    /* Initialize the clock monitor core - it monitors the frequencies of all clocks coming to the FPGA.
       We use it to self-diagnose if the board's oscillators are working correctly. */
    ertm14_clock_monitor_init();


    if( ! (board.mode & ERTM14_MODE_WITHOUT_ERTM15 ) )
    {
        /* Set up the eRTM15's PLL */
        ertm15_pll_init();
    }

    /* Set up the eRTM14's PLLs (AD9516s) */
    ertm14_init_ref_clock_distribution();

    /* At this point, we should have a stable CLK_REF coming from the PLL. Tell the FPGA to use it also as the system clock */
    board_dbg("Switching system clock to CLK_SYS\n");
    ertm14_switch_sys_clock(1);

    /* Disable bit-banged OCXO control (used for debug) */
    gen_gpio_out(&pin_ocxo_override, 0);

    /* Create a debug SPI master for testing the OCXO tuning. Normally it's driven in hardware by the SoftPLL, I left
       this device for debugging purposes. It's active if GPIO pin ocxo_override == 1 */
    bb_spi_create( &board.spi_ocxo_dac,
        &pin_ocxo_cs_n,
        &pin_ocxo_data,
        &pin_ocxo_data,
        &pin_ocxo_sclk,
        100 );


    /* Read unique MAC addresses from storage chips (eRTM14 - IC7 and IC8) */
    ertm14_init_mac_eeprom();

    /* RF Power Monitor ADC (eRTM15 - IC43) */
    bb_spi_create( &board.spi_ad7888,
        &pin_pwrmon_adc_cs_n,
        &pin_pwrmon_adc_din,
        &pin_pwrmon_adc_dout,
        &pin_pwrmon_adc_sclk,
        100 );

    ad7888_create( &board.pwrmon_adc, &board.spi_ad7888 );

    if( ! (board.mode & ERTM14_MODE_WITHOUT_ERTM15 ) )
    {
        board_dbg("Initializing RF distribution\n");


    /* RF distribution switches and shift registers controlling these (eRTM15 - IC26..28) */
        ertm15_rf_distr_init( &board.rf_distr, &board.pwrmon_adc );

    /* Now that the PLL clocks are ready, init the DDS synthesizers */
        board_dbg("Initializing DDSes\n");
        ertm15_init_dds();

        /* Program the DDSes to some meaninfgul settings, say, 205 MHz */
        ad9910_program(&board.dds_ad9910_ref, 205000000ULL, 0, 0x0 );
        ad9910_program(&board.dds_ad9910_lo, 205000000ULL, 0, 0x0 );
    }

    /* Setup the SoftPLL for the OCXO we have */
    ertm14_spll_setup();

    if( ! (board.mode & ERTM14_MODE_WITHOUT_ERTM15 ) )
    {
        /* Init CLKA/CLKB distribution (AD9520s) */
        board_dbg("Initializing CLKA/CLKB distribution...\n");
        ertm14_init_clkab_distribution();
        board_dbg("Calibrating DDS sync pulse...\n");
        ertm14_dds_sync_calibrate();
    }

    /* Initialize the IUART which is responsible for the communication with the MMC.
       Fixme: below is IUART14 which talks to the MMC on eRTM14. If eRTM15 is present, we need another IUART device. */
    board_dbg("Init IUART14\n");

    iuart_init_bare( &board.iuart_14, BASE_IUART_14, 115200 );

    board_dbg("eRTM14/15 early init done\n");


    board_dbg("Init RF transceiver\n");
    wr_rf_frame_transceiver_create( &board.rf_xcvr, BASE_ERTM14_RF_FRAME_TRANSCEIVER );
    
    ertm_init_complete = 1;

    return 0;
}



void ertm14_config_init()
{
    int i, j;

    for(i = 0; i < ERTM14_MAX_CONFIGS; i++)
    {
        struct ertm14_board_state *cfg = &ertm14_configs[i];

        cfg->valid = 1;
        cfg->lo.ftw = 0x39374BC6;
        cfg->ref.ftw =  0x39374BC6;
        cfg->lo.ampl_factor = 50;
        cfg->ref.ampl_factor = 50; 

        for( j = 0; j <= ERTM14_RF_OUT_MAX_ID; j++)
        {
            cfg->ref.out_state [j] = ERTM15_RF_OUT_MONITOR;
            cfg->lo.out_state [j] = ERTM15_RF_OUT_MONITOR;
        }
    
        for(j = 0; j <= ERTM14_CLKAB_OUT_MAX_ID; j++)
        {
            cfg->clka_freq_hz[j] = 500000000;
            cfg->clkb_freq_hz[j] = 500000000;
        }

        cfg->clka_enable_mask = -1; //( 1<<11);
        cfg->clkb_enable_mask = -1; //( 1<<11);
    }
};

struct ertm14_board_state *ertm14_get_state_for_config(int config_id)
{
    return &ertm14_configs[config_id];
}


int ertm14_apply_config(int config_id)
{
    ertm14_current_state = &ertm14_configs[config_id];
    has_new_config = 1;
}

int ertm14_get_current_config_id()
{
    int i;

    for(i = 0; i < ERTM14_MAX_CONFIGS; i++)
    if( &ertm14_configs[i] == ertm14_current_state )
            return i;

    return -1;
}

static int ertm14_commit_config( struct  ertm14_board_state *cfg )
{  
    int i;
        for( i = 0; i <= ERTM14_CLKAB_OUT_MAX_ID; i++)
        {

            // digital clocks

            int freq_a = cfg->clka_freq_hz[i];
            int freq_b = cfg->clkb_freq_hz[i];
            int div_a = ertm14_get_clkab_divider( freq_a );
            int div_b = ertm14_get_clkab_divider( freq_b );
            int enable_a = ( cfg->clka_enable_mask & (1<<i) ) ? 1 : 0;
            int enable_b = ( cfg->clkb_enable_mask & (1<<i) ) ? 1 : 0;

            pp_printf("CLKA%d: freq=%d Hz, divider=%d, enable=%d\n", i, freq_a, div_a, enable_a);
            pp_printf("CLKA%d: freq=%d Hz, divider=%d, enable=%d\n", i, freq_b, div_b, enable_b);

            ad9520_set_output_divider( &board.dev_clka_distr, i, div_a ); // divide by 4 -> 250 MHz
            ad9520_set_output_divider( &board.dev_clkb_distr, i, div_b );

            ad9520_enable_output( &board.dev_clka_distr, i, enable_a );
            ad9520_enable_output( &board.dev_clkb_distr, i, enable_b );
        }

            // DDSes

        ad9910_program(&board.dds_ad9910_lo, cfg->lo.ftw, 0, cfg->lo.ampl_factor );
        ad9910_program(&board.dds_ad9910_ref, cfg->ref.ftw, 0, cfg->ref.ampl_factor );

        pp_printf("DDS LO: FTW=0x%08x, ampl=%d\n", cfg->lo.ftw, cfg->lo.ampl_factor );
        pp_printf("DDS REF: FTW=0x%08x, ampl=%d\n", cfg->ref.ftw, cfg->ref.ampl_factor );
        
        for( i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
        {
            int st_lo = cfg->lo.out_state[i] == ERTM15_RF_OUT_ON ? 1 : 0;
            int st_ref = cfg->ref.out_state[i] == ERTM15_RF_OUT_ON ? 1 : 0;
            pp_printf("i %d lo %x ref %x\n", i, st_lo, st_ref );

            ertm15_rf_distr_output_enable( &board.rf_distr, ERTM15_RF_LO, i, st_lo );
            ertm15_rf_distr_output_enable( &board.rf_distr, ERTM15_RF_REF, i, st_ref );
        }

        ertm15_update_rf_switches( &board.rf_distr );
}

static int ertm14_update_config_task(void)
{
    if (has_new_config && ertm_init_complete && ertm14_current_state->valid)
    {
        int i;
        pp_printf("New config detected, applying...\n");

        has_new_config = 0;

        if (!(board.mode & ERTM14_MODE_WITHOUT_ERTM15))
        {
            ertm14_commit_config(ertm14_current_state);
            ertm14_clk_pps_sync_restart();
        }
    }
}

static struct {
    int freq;
    int divider;
} clkab_freqs [] = {
    { 1000000000, 1 },
    { 500000000, 2},
    { 250000000, 4},
    { 100000000, 5},
    { 125000000, 8},
    { 62500000, 16},
    {-1,-1}
};

int ertm14_get_clkab_divider( int freq )
{
    int i;
    for (i=0;clkab_freqs[i].freq >= 0; i++)
    {
        if (clkab_freqs[i].freq == freq)
            return clkab_freqs[i].divider;
    }
    
    return -1;
}

int ertm14_get_supported_clkab_freqs( int *freqs, int max_count )
{
    int i;
    for(i = 0;clkab_freqs[i].freq >= 0; i++)
    {
        if(  i < max_count )
        {
            freqs [i] = clkab_freqs[i].freq;
        } else
            break;
    }

    return i;
}

int wrc_board_early_init()
{
    int32_t flash_entry_points[64];
    int i;
    /* initialize SPI flash */
	flash_init();

	/* initialize I2C bus */
	bb_i2c_init( &dev_i2c_fmc );
   

    for(i = 0; i < 32; i++)
        flash_entry_points[i] = 0x800000 + 0x40000 * i;

    flash_entry_points[i] = -1;

    
    /* init storage (we use the SPI flash on eRTM14) */
    storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
    wrc_storage_dev.entry_points = &flash_entry_points[0];
    storage_mount( &wrc_storage_dev );

    /* reset the networking part of the WRCore and start the WR Endpoint */
   	net_rst();
	ep_init();

    /* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
    // fixme: not sure this is necessary in eRTM14 but it doesn't hurt - TW
	timer_delay_ms(200);
	ep_enable(1, 1);
	timer_delay_ms(200);

    return ertm14_init();
}

int wrc_board_init()
{
    ertm14_shell_init();

    return 0;
}

timeout_t rf_nco_sync_tmo;

static void rf_nco_sync_init()
{
    tmo_init( &rf_nco_sync_tmo, 5 );
}

void rf_nco_sync_poll()
{
    if (tmo_expired(&rf_nco_sync_tmo))
    {
          tmo_restart(&rf_nco_sync_tmo);

    }
}

extern int phy_calibration_poll();
extern void phy_calibration_init();

int wrc_board_create_tasks()
{
    wrc_task_create( "iuart14", NULL, iuart_14_poll );
    wrc_task_create( "clk-pps-sync", ertm14_clk_pps_sync_init, ertm14_clk_pps_sync_task );
    wrc_task_create( "ertm-config", NULL, ertm14_update_config_task );
    wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );

    wrc_task_create( "rf_nco_sync", rf_nco_sync_init, rf_nco_sync_poll );

    return 0;
}
