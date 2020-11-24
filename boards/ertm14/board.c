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
#include "dev/ltc695x.h"
#include "dev/ad9910.h"
#include "dev/clock_monitor.h"
#include "dev/24aa025.h"
#include "dev/ad7888.h"
#include "dev/spi_flash.h"
#include "dev/bb_i2c.h"
#include "dev/pps_gen.h"
#include "dev/console.h"
#include "dev/endpoint.h"
#include "dev/74x595.h"
#include "dev/netif.h"

#include "lib/ertm14-uart-link.h"

#include "softpll_ng.h"
#include "storage.h"
#include "wrc_ptp.h"

#include <hw/wr_streamers.h>
#include <wrc-event.h>

#include "ertm15_rf_distr.h"
#include "rf_frame_transceiver.h"

// allows the eRTM14 board to operate *without* the eRTM15 (no WR support, useful for IPMI testing)
#undef CONFIG_ERTM14_WITHOUT_ERTM15

#include "hw/wb_10mhz_align_unit.h"
#include "wrc-task.h"

#include <errno.h>

struct ertm14_board board;
struct ertm14_board_state ertm14_configs[ ERTM14_MAX_CONFIGS ];
struct ertm14_board_state *ertm14_current_state;

struct gpio_pin pin_pll_main_cs_n = { &board.gpio_aux, 0 };
struct gpio_pin pin_pll_main_sdi = { &board.gpio_aux, 1 };
struct gpio_pin pin_pll_main_sdo = { &board.gpio_aux, 2 };
struct gpio_pin pin_pll_main_sclk = { &board.gpio_aux, 3 };
struct gpio_pin pin_pll_main_reset = { &board.gpio_aux, 4 };
struct gpio_pin pin_pll_main_lock = { &board.gpio_aux, 5 };

struct gpio_pin pin_pll_ext_cs_n = { &board.gpio_aux, 6 };
struct gpio_pin pin_pll_ext_sdi = { &board.gpio_aux, 7 };
struct gpio_pin pin_pll_ext_sdo = { &board.gpio_aux, 8 };
struct gpio_pin pin_pll_ext_sclk = { &board.gpio_aux, 9 };
struct gpio_pin pin_pll_ext_reset = { &board.gpio_aux, 10 };
struct gpio_pin pin_pll_ext_lock = { &board.gpio_aux, 11 };

struct gpio_pin pin_mac_addr_scl = { &board.gpio_aux, 13 };
struct gpio_pin pin_mac_addr_sda = { &board.gpio_aux, 12 };

struct gpio_pin pin_main_xo_en_n = { &board.gpio_aux, 14 };

struct gpio_pin pin_ltc6950_sclk = { &board.gpio_aux, 15 };
struct gpio_pin pin_ltc6950_sdi = { &board.gpio_aux, 16 };
struct gpio_pin pin_ltc6950_sdo = { &board.gpio_aux, 17 };
struct gpio_pin pin_ltc6950_ce_gen = { &board.gpio_aux, 18 };
struct gpio_pin pin_ltc6950_ce_distr = { &board.gpio_aux, 19 };
struct gpio_pin pin_ltc6950_sync = { &board.gpio_aux, 20 };

struct gpio_pin pin_ad9910_lo_sdio = { &board.gpio_aux, 4+21 };
struct gpio_pin pin_ad9910_lo_sclk = { &board.gpio_aux, 5+21 };
struct gpio_pin pin_ad9910_lo_reset = { &board.gpio_aux, 6+21 };
struct gpio_pin pin_ad9910_lo_io_update = { &board.gpio_aux, 3+21 };
const struct gpio_pin pin_ad9910_lo_sync_smp_err = { &board.gpio_aux, 5+21 };

struct gpio_pin pin_ad9910_ref_sdio = { &board.gpio_aux, 4+28 };
struct gpio_pin pin_ad9910_ref_sclk = { &board.gpio_aux, 5+28 };
struct gpio_pin pin_ad9910_ref_reset = { &board.gpio_aux, 6+28 };
struct gpio_pin pin_ad9910_ref_io_update = { &board.gpio_aux, 3+28 };
const struct gpio_pin pin_ad9910_ref_sync_smp_err = { &board.gpio_aux, 5+28 };

struct gpio_pin pin_ocxo_override = { &board.gpio_aux, 48 };
struct gpio_pin pin_ocxo_cs_n = { &board.gpio_aux, 51 };
struct gpio_pin pin_ocxo_sclk = { &board.gpio_aux, 50 };
struct gpio_pin pin_ocxo_data = { &board.gpio_aux, 49 };

struct gpio_pin pin_pwrmon_adc_cs_n = {  &board.gpio_aux, 52 };
struct gpio_pin pin_pwrmon_adc_dout = {  &board.gpio_aux, 46 };
struct gpio_pin pin_pwrmon_adc_din = {  &board.gpio_aux, 47 };
struct gpio_pin pin_pwrmon_adc_sclk = {  &board.gpio_aux, 45 };

struct gpio_pin pin_sys_clk_sel_stb = {  &board.gpio_aux, 61 };
struct gpio_pin pin_sys_clk_sel_next = {  &board.gpio_aux, 62 };

struct gpio_pin pin_pps_out_mode0 = {  &board.gpio_aux, 63 };
struct gpio_pin pin_pps_out_mode1 = {  &board.gpio_aux, 64 };
struct gpio_pin pin_pps_out_mode2 = {  &board.gpio_aux, 65 };

struct gpio_pin pin_led_sync_green = {  &board.gpio_aux, 69 };
struct gpio_pin pin_led_sync_red = {  &board.gpio_aux, 70 };

struct gpio_pin pin_ertm15_leds_ser = { &board.gpio_aux, 66 };
struct gpio_pin pin_ertm15_leds_updtclk = { &board.gpio_aux, 67 };
struct gpio_pin pin_ertm15_leds_shftclk = { &board.gpio_aux, 68 };


// a/b/lo/ref, red->green
struct gpio_pin pin_ertm15_led_clka_red = { &board.gpio_ertm15_leds, 0 };
struct gpio_pin pin_ertm15_led_clka_green = { &board.gpio_ertm15_leds, 1 };
struct gpio_pin pin_ertm15_led_clkb_red = { &board.gpio_ertm15_leds, 2 };
struct gpio_pin pin_ertm15_led_clkb_green = { &board.gpio_ertm15_leds, 3 };
struct gpio_pin pin_ertm15_led_lo_red = { &board.gpio_ertm15_leds, 4 };
struct gpio_pin pin_ertm15_led_lo_green = { &board.gpio_ertm15_leds, 5 };
struct gpio_pin pin_ertm15_led_ref_red = { &board.gpio_ertm15_leds, 6 };
struct gpio_pin pin_ertm15_led_ref_green = { &board.gpio_ertm15_leds, 7 };

struct gpio_pin pin_ertm15_clkab_mosi = { &board.gpio_aux, 57 };
struct gpio_pin pin_ertm15_clkab_miso = { &board.gpio_aux, 57 };
struct gpio_pin pin_ertm15_clkab_sck = { &board.gpio_aux, 58 };
struct gpio_pin pin_ertm15_clka_cs_n = { &board.gpio_aux, 59 };
struct gpio_pin pin_ertm15_clkb_cs_n = { &board.gpio_aux, 60 };

struct ad95xx_config pll_ext_10mhz_config = 
#include "configs/ertm_14_pll_ext_10mhz.h"

struct ad95xx_config pll_main_dot050_config =
#include "configs/ertm_14_pll_main_dot050_config.h"

struct ad95xx_config pll_main_ocxo_config =
#include "configs/ertm_14_pll_ocxo_config.h"

struct ltc695x_config pll_ertm15_bootstrap_config =
#include "configs/ertm_15_ltc6950_config_rev2.h"

struct ltc695x_config clkab_ertm15_bootstrap_config =
#include "configs/ertm_15_ltc6953_bootstrap_config.h"

spll_gain_schedule_t spll_main_ocxo_gain_sched;

#define ERTM14_BIST_LTC6950 0
#define ERTM14_BIST_MAC_EEPROM 1
#define ERTM14_BIST_AD951X_MAIN 2
#define ERTM14_BIST_AD951X_EXT 3
#define ERTM14_BIST_MAIN_OCXO 4
#define ERTM14_BIST_DMTD_VCXO 5
#define ERTM14_BIST_CLKA 6
#define ERTM14_BIST_CLKB 7
#define ERTM14_BIST_DDS_REF 8
#define ERTM14_BIST_DDS_LO 9
#define ERTM14_BIST_FLASH_PRESENCE 10
#define ERTM14_BIST_FLASH_FS_MOUNT 11

#define BIST_STATUS_DONE (1<<0)
#define BIST_STATUS_ERROR (1<<1)

struct bist_stage
{
    uint8_t id;
    const char *name;
    uint8_t n_channels;
    uint64_t status;
};

static struct bist_stage ertm_bist[] = {
    {ERTM14_BIST_FLASH_PRESENCE, "Check flash presence", 1},
    {ERTM14_BIST_FLASH_FS_MOUNT, "Mount flash FS", 1},
    {ERTM14_BIST_LTC6950, "LTC6950", 1},
    {ERTM14_BIST_MAC_EEPROM, "MAC EEPROM", 1},
    {ERTM14_BIST_AD951X_EXT, "AD9510 (Ext)", 1},
    {ERTM14_BIST_AD951X_MAIN, "AD9510 (Main)", 1},
    {ERTM14_BIST_CLKA, "LTC6953 (CLKA fanout)", 1},
    {ERTM14_BIST_CLKB, "LTC6953 (CLKB fanout)", 1},
    {ERTM14_BIST_DDS_LO, "DDS comm (LO)", 1},
    {ERTM14_BIST_DDS_REF, "DDS comm (REF)", 1},
    {0, NULL}};


void bist_checkpoint( struct bist_stage *bist, int id, int channel, int pass )
{
    int i;
    for(i = 0; bist[i].name; i++ )
        if( bist[i].id == id )
        {
            bist[i].status &= ~(3 << (channel * 2) );

            if(!pass)
                bist[i].status |= BIST_STATUS_ERROR << (channel * 2);
            bist[i].status |= BIST_STATUS_DONE << (channel * 2);
        }
}

void bist_init( struct bist_stage *bist )
{
    int i;
    for(i = 0; bist[i].name; i++ )
        bist[i].status = 0;
}

int bist_summary( struct bist_stage *bist )
{
    int i;
    int n_ok = 0, n_errors = 0;
    pp_printf("Built-in Self Test Summary\n------------------------------\n");
    pp_printf("Id  | Test name                       | Channel | Status       \n");

    for(i = 0; bist[i].name; i++ )
    {
        int ch;
        struct bist_stage *s = &bist[i];

        for( ch = 0; ch < s->n_channels; ch++ )
        {
            pp_printf("%-3d | %-31s | ", i + 1, bist[i].name);
            if( s->n_channels > 1 )
                pp_printf("%-2d    | ", ch );
            else
                pp_printf("-       | ");

            int stat = s->status >> (ch * 2);

            if( !( stat & BIST_STATUS_DONE ) )
                pp_printf("Not ran");
            else if (stat & BIST_STATUS_ERROR)
            {
                pp_printf("ERROR");
                n_errors++;
            }
            else
            {
                pp_printf("OK");
                n_ok++;
            }

            pp_printf("\n");
        }
    }

    if( n_errors )
        pp_printf("--------------------------------\nBIST FAILED with %d ERRORS!\n\n\n", n_errors );
    else 
        pp_printf("BIST PASSED.\n");

    return n_errors > 0 ? -1 : 0;
}

static int ertm_init_complete = 0;

void ertm14_set_pps_out_mode(int mode);

#define LTC6950_ID_VALUE 0x65

// fixme: use PRESENCE_A/B pins instead of LTC6950 PLL chip
static int check_ertm15_presence(void)
{
    ltc695x_init(&board.ltc6950_pll, &board.spi_ltc6950);

    int id = ltc695x_read( &board.ltc6950_pll, 0x16 );

    board_dbg("detect LTC6950: ID %x should be %x\n", id, LTC6950_ID_VALUE );

    if( id != LTC6950_ID_VALUE )
        return 0;

    return 1;
}

/* CLKA inverted outputs: 0, 1, 4, 5, 6 (LTC6953 ordering) */
/* CLKB inverted outputs: 2, 7, 8, 9 (LTC6953 ordering) */

struct clkab_output_map_entry
{
    int8_t id_ltc6953;
    int8_t id_backplane;
    uint8_t invert;
};

/* Backplane output mapping:

   LTC6953 Output        BP Output      Invert
   0                     CLKA10            x
   1                     CLKA11            x
   2                     CLKA9
   3                     CLKA8
   4                     CLKA7             x
   5                     CLKA6             x
   6                     CLKA12            x
   7                     CLKA5
   8                     CLKA4
   9                     CLKA14
   10                    CLKA-FP
*/

static const struct clkab_output_map_entry clka_out_map[] =
    {
        {0, 10, 1},
        {1, 11, 1},
        {2, 9, 0},
        {3, 8, 0},
        {4, 7, 1},
        {5, 6, 1},
        {6, 12, 1},
        {7, 5, 0},
        {8, 4, 0},
        {9, 14, 0},
        {10, ERTM14_CLKAB_OUT_FRONT_PANEL, 0},
        {-1, -1, 0}
};

/* LTC6953 Output        BP Output      Invert
   0                     CLKB14
   1                     CLKB10
   2                     CLKB12             x
   3                     CLKB11
   4                     CLKB9
   5                     CLKB8
   6                     CLKB7
   7                     CLKB6             x
   8                     CLKB5             x
   9                     CLKB4             x
   10                    CLKB-FP
*/


static const struct clkab_output_map_entry clkb_out_map[] =
    {
        {0, 14, 0},
        {1, 10, 0},
        {2, 12, 1},
        {3, 11, 0},
        {4, 9, 0},
        {5, 8, 0},
        {6, 7, 0},
        {7, 6, 1},
        {8, 5, 1},
        {9, 4, 1},
        {10, ERTM14_CLKAB_OUT_FRONT_PANEL, 0},
        {-1, -1, 0}
};


static void ertm14_spll_setup(void)
{
/* configure a suitable PI gain schedule for the SoftPLL: */
    spll_gain_schedule_t* gs=  &spll_main_ocxo_gain_sched;

    gs->n_stages = 1;

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
        uint32_t val = board.dds_sync_delays[ params[i].channel ];
        storage_get_calibration_parameter( params[i].id, &val );
        board_dbg("Sync Unit channel '%s': delay = %d ps\n", params[i].name, val);
    }

// Sync_in: continuous waveform, use external delay line (inside AD9910)
    
    // produce a continuos sync clock for the DDSes
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 1, board.dds_sync_delays[ERTM14_DDS_SYNC_LO], FINE_PULSE_GEN_CONTINUOUS );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 1, board.dds_sync_delays[ERTM14_DDS_SYNC_REF], FINE_PULSE_GEN_CONTINUOUS );
    fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_REF, 75, ad9910_set_fine_delay );
    fine_pulse_gen_set_external_fine_delay( &board.dds_sync_dev, ERTM14_DDS_SYNC_LO, 75, ad9910_set_fine_delay );

    board_dbg("ref delay = %d lo delay = %d\n", board.dds_sync_delays[ERTM14_DDS_IOUPDATE_REF], board.dds_sync_delays[ERTM14_DDS_IOUPDATE_LO] );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_LO, 1, board.dds_sync_delays[ERTM14_DDS_IOUPDATE_LO], 0 );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_DDS_IOUPDATE_REF, 1, board.dds_sync_delays[ERTM14_DDS_IOUPDATE_REF], 0 );

// CLKAB Sync: internal delay line, single-shot mode, negative polarity
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKA, 1, board.dds_sync_delays[ERTM14_PLL_SYNC_CLKA], FINE_PULSE_GEN_NEGATIVE );
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKB, 1, board.dds_sync_delays[ERTM14_PLL_SYNC_CLKB], FINE_PULSE_GEN_NEGATIVE );
    
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
    

    board_dbg("DDS_LO SYNC start=%d ps length=%d ps setpoint=%d ps\n",
        windows[0].best_start, windows[0].best_length, windows[0].setpoint
    );
    board_dbg("DDS_REF SYNC start=%d ps length=%d ps setpoint=%d ps\n",
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
    board_dbg("1\n");

// now that we are done syncing DDS internal clocks, disable the fpga SYNC_CLK output
    if( ! ( board.mode & ERTM14_MODE_WITHOUT_ERTM15 ) )
    {
        ad9910_configure_sync( &board.dds_ad9910_ref, 0, 0 );
        ad9910_configure_sync( &board.dds_ad9910_lo, 0, 0 );
    }

    channel_mask = (1 << ERTM14_PLL_SYNC_CLKA) |
                   (1 << ERTM14_PLL_SYNC_CLKB);



    // fixme: trigger clkab sync to pps

    board_dbg( "CLKAB sync complete\n");

    return 0;
}

void blink(int id)
{
    struct gpio_pin *pin = NULL;

    if(id == 0 )
        pin = &pin_ertm15_led_lo_green;
    else if (id == 1 )
        pin = &pin_ertm15_led_lo_red;
    else if (id == 2 )
        pin = &pin_ertm15_led_ref_red;
    
    gen_gpio_out( pin, 1 );
    timer_delay_ms(50);
    gen_gpio_out( pin, 0 );
    timer_delay_ms(50);
}

static void control_uart_mode_callback( int is_binary )
{
    if( is_binary )
        uart_link_reset( &board.control_uart_link );
}

static int control_uart_poll(void)
{
    struct uart_packet *pkt;

    if( uart_link_recv( &board.control_uart_link, &pkt, 0 ) > 0 )
    {
        struct uart_packet tx_pkt;
        /*... dispatch */
        if( pkt->ptype == ERTM14_UART_PTYPE_PING )
        {
            tx_pkt.ptype = ERTM14_UART_PTYPE_PING;
            tx_pkt.length = 10;

            uart_link_send( &board.control_uart_link, &tx_pkt );

            blink(1);
        }
    }

    return 0;
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
        writel( TAU_CSR_TRIG, (void*) TAU_REG_CSR + BASE_ERTM14_10MHZ_ALIGN_UNIT );
        uint32_t csr = readl( (void*) TAU_REG_CSR + BASE_ERTM14_10MHZ_ALIGN_UNIT);
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

static int evth_dds_nco_sync;

#define DDS_NCO_STATE_WAIT_TIMING 0
#define DDS_NCO_STATE_RECONFIGURE 1
#define DDS_NCO_STATE_ARM 2
#define DDS_NCO_STATE_WAIT_TRIGGER 3

static int dds_nco_sync_state = 0;

static void ertm14_dds_nco_sync_init(void)
{
   dds_nco_sync_state = DDS_NCO_STATE_WAIT_TIMING;
}


static void rf_nco_sync_disable_channel( struct ertm14_dds_state *state, uint32_t ioupdate_channel )
{
    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ioupdate_channel, 1, board.dds_sync_delays[ioupdate_channel], 0  );
    state->sync_count = 0;
}

static void rf_nco_sync_configure_channel( struct ertm14_dds_state *state, uint32_t ioupdate_channel )
{
    int flags = 0;

    if( state->sync_source == ERTM14_SYNC_SOURCE_RF_TRIGGER)
        flags |= FINE_PULSE_GEN_USE_EXT_TRIGGER;

    fine_pulse_gen_setup_channel ( &board.dds_sync_dev, ioupdate_channel, 1, board.dds_sync_delays[ioupdate_channel], flags  );
    state->sync_count = 0;
}

static void rf_nco_sync_arm_channel( struct ertm14_dds_state *state, uint32_t ioupdate_channel )
{
    if( state->sync_source == ERTM14_SYNC_SOURCE_NONE)
        return;

    fine_pulse_gen_trigger ( &board.dds_sync_dev, (1 << ioupdate_channel), 0 );
}


static int rf_nco_sync_wait_trigger( struct ertm14_dds_state *state, uint32_t ioupdate_channel )
{
    // no sync? consider the channel always triggered
    if( state->sync_source == ERTM14_SYNC_SOURCE_NONE)
        return 1;

    if( fine_pulse_gen_is_triggered ( &board.dds_sync_dev, 1 << ioupdate_channel ) )
    {
        state->sync_count++;
        return 1;
    }

    return 0;
}

static int ertm14_dds_nco_sync_task(void)
{
    int evt = event_poll( evth_dds_nco_sync );

    switch( evt )
    {
        case WRC_ERTM14_EVENT_RECONFIGURED:
            board_dbg("nco_sync: reconfig request\n");
            dds_nco_sync_state = DDS_NCO_STATE_RECONFIGURE;
            break;
        case WRC_EVENT_LINK_DOWN:
        case WRC_EVENT_LINK_UP:
        case WRC_EVENT_TIMING_DOWN:
        case WRC_EVENT_TIMING_UP:
            board_dbg("nco_sync: link/timing status change, restarting\n");
            rf_nco_sync_disable_channel( &ertm14_current_state->ref, ERTM14_DDS_IOUPDATE_REF );
            rf_nco_sync_disable_channel( &ertm14_current_state->lo, ERTM14_DDS_IOUPDATE_LO );
            dds_nco_sync_state = DDS_NCO_STATE_WAIT_TIMING;
            break;
        default:
            break;
    }

    

    switch( dds_nco_sync_state )
    {
        case DDS_NCO_STATE_WAIT_TIMING:
            if( evt == WRC_EVENT_TIMING_UP)
            {
                board_dbg("nco_sync: timing up, configuring FPGen\n");
                dds_nco_sync_state = DDS_NCO_STATE_RECONFIGURE;
            }
            break;

        case DDS_NCO_STATE_RECONFIGURE:
            if( !wrc_is_timing_up() )
            {
                dds_nco_sync_state = DDS_NCO_STATE_WAIT_TIMING;
            } else {
                rf_nco_sync_configure_channel( &ertm14_current_state->ref, ERTM14_DDS_IOUPDATE_REF );
                rf_nco_sync_configure_channel( &ertm14_current_state->lo, ERTM14_DDS_IOUPDATE_LO );
                ertm14_set_pps_out_mode( 3 ); // observe RF reset NCO triggers on PPS out
                dds_nco_sync_state = DDS_NCO_STATE_ARM;
            }
            break;

        case DDS_NCO_STATE_ARM:
            //board_dbg("(Arm!)\n");
            rf_nco_sync_arm_channel( &ertm14_current_state->ref, ERTM14_DDS_IOUPDATE_REF );
            rf_nco_sync_arm_channel( &ertm14_current_state->lo, ERTM14_DDS_IOUPDATE_LO );
            dds_nco_sync_state = DDS_NCO_STATE_WAIT_TRIGGER;
            break;
        case DDS_NCO_STATE_WAIT_TRIGGER:
        {
            int trig_ref = rf_nco_sync_wait_trigger( &ertm14_current_state->ref, ERTM14_DDS_IOUPDATE_REF );
            int trig_lo = rf_nco_sync_wait_trigger( &ertm14_current_state->lo, ERTM14_DDS_IOUPDATE_REF );


            if( trig_ref && trig_lo )
            {
                //board_dbg("(Trig!)\n");
                dds_nco_sync_state = DDS_NCO_STATE_ARM;
            }

            break;
        }

        default:
            break;
    }

    return 0;
}

// fixme: factor out all this code to a common file (used by sis83k, afcz, ertm)
static int calc_apr(int meas_min, int meas_max, int f_center )
{
	// apr_min is in PPM

	if( f_center < meas_min || f_center > meas_max )
		f_center = (meas_min + meas_max) / 2;

	int64_t delta_low =  meas_min - f_center;
	int64_t delta_hi = meas_max - f_center;
	uint64_t u_delta_low, u_delta_hi;
	int ppm_lo, ppm_hi;

	if(delta_low >= 0)
		return -1;
	if(delta_hi <= 0)
		return -1;

	/* __div64_32 divides 64 by 32; result is in the 64 argument. */
	u_delta_low = -delta_low * 1000000LL;
	__div64_32(&u_delta_low, f_center);
	ppm_lo = (int)u_delta_low;

	u_delta_hi = delta_hi * 1000000LL;
	__div64_32(&u_delta_hi, f_center);
	ppm_hi = (int)u_delta_hi;

	return ppm_lo < ppm_hi ? ppm_lo : ppm_hi;
}

static int measure_vcxo_freq( int cm_channel, int cm_ref, int gate_freq, int n_steps, uint32_t expected_freq, void (*dac_setter)(int), int *apr, uint32_t *base_freq )
{
	int f_min = 0, f_max = 0;
	int tune_min = 0;
	int tune_max = 65535;
	int tune_step = (tune_max-tune_min) / n_steps;

	wb_cm_configure( &board.ertm14_cmon, cm_ref, 5, gate_freq );
	wb_cm_set_ref_frequency( &board.ertm14_cmon, CPU_CLOCK );

	int tune = tune_min;

	for(;;)
	{

		dac_setter( tune );
		timer_delay_ms(1);
		wb_cm_restart( &board.ertm14_cmon );
		while( ! (wb_cm_read( &board.ertm14_cmon ) & ( 1<< cm_channel) ) );

		int f = board.ertm14_cmon.freqs[ cm_channel ];

		if( tune == tune_min )
			f_min = f;
		else if ( tune == tune_max )
			f_max = f;

		if(tune == tune_max)
			break;

		board_dbg("Tune: %d f = %d Hz (deltaF = %d Hz)\n", tune, f, f - expected_freq );

		tune += tune_step;
		if( tune > tune_max )
			tune = tune_max;
	}

	dac_setter( 32768 );
	timer_delay(1);

    int l_apr = calc_apr(f_min, f_max, 62500000);

    if( apr )
        *apr = l_apr;

    if( base_freq )
        *base_freq = (f_min + f_max) / 2;

    board_dbg("VCO ch %d:  Low=%d Hz Hi=%d Hz, APR = %d ppm.\n", cm_channel, f_min, f_max, l_apr );

    return 0;
}


static void blink_led( struct gpio_pin *pin )
{
    gen_gpio_out( pin, 1 );
    timer_delay_ms(150);
    gen_gpio_out( pin, 0 );
}

static void ertm14_test_leds(void)
{
    blink_led( &pin_led_sync_green );
    blink_led( &pin_led_sync_red );
}

static void ertm15_test_leds(void)
{
    blink_led(&pin_ertm15_led_ref_green);
    blink_led(&pin_ertm15_led_lo_green);
    blink_led(&pin_ertm15_led_clkb_green);
    blink_led(&pin_ertm15_led_clka_green);
    blink_led(&pin_ertm15_led_ref_red);
    blink_led(&pin_ertm15_led_lo_red);
    blink_led(&pin_ertm15_led_clkb_red);
    blink_led(&pin_ertm15_led_clka_red);
}

static void set_main_dac( int value )
{
	spll_set_dac( 0, value );
}

static void set_dmtd_dac( int value )
{
	spll_set_dac( -1, value );
}

int ertm15_check_oscillators(void)
{
    board_dbg("Check REF OCXO\n");
    measure_vcxo_freq( ERTM14_CMON_CLK_REF, ERTM14_CMON_CLK_DMTD, 10000000, 1, 62500000, set_main_dac, NULL, NULL );
    board_dbg("Check DMTD VCXO\n");
    measure_vcxo_freq( ERTM14_CMON_CLK_DMTD, ERTM14_CMON_CLK_REF, 100000, 10, 62500000, set_dmtd_dac, NULL, NULL );
    return 0;
}

// initializes the eRTM15 LTC6950 PLL & OCXO
int ertm15_pll_init(void)
{
    ltc695x_init(&board.ltc6950_pll, &board.spi_ltc6950);

    int id = ltc695x_read(&board.ltc6950_pll, 0x16);

    if (id != LTC6950_ID_VALUE)
    {
        board_dbg("Error initializing LTC6950 (read RevID: 0x%x, expected: 0x%x)\n", id, LTC6950_ID_VALUE);
        return -1;
    }

    bist_checkpoint( ertm_bist, ERTM14_BIST_LTC6950, 0, id == LTC6950_ID_VALUE);

    // load default 'bootstrap' config and check what is the OCXO frequency
    ltc695x_configure(&board.ltc6950_pll, &pll_ertm15_bootstrap_config);

    board_dbg("Using 100 MHz OCXO\n");
    //ltc6950_write( &board.ltc6950_pll, 0x15, 4 ); // RDIVOUT = 0, output div = 50
    ltc695x_write(&board.ltc6950_pll, 0x8, 0x1); // reference divider = 1

    ltc695x_write(&board.ltc6950_pll, 0x15, 50); // RDIVOUT = 0, output div = 50
    ltc695x_write(&board.ltc6950_pll, 0x0a, 10); // N divider = 10 (VCO @ 1GHz, PFD @ 10 MHz)
    board.mode |= ERTM14_MODE_OCXO_100MHZ;
    return 0;
}

static const struct clkab_output_map_entry *clkab_find_map_entry(  int clka_or_clkb, int output )
{
    const struct clkab_output_map_entry *omap = (clka_or_clkb == ERTM14_OUT_CLKA) ? clka_out_map : clkb_out_map;
    int i;
    for( i = 0; omap[i].id_backplane >= 0; i++ )
    {
        if( omap[i].id_backplane  == output )
            return &omap[i];
    }

    return NULL;
}

static int clkab_set_output_divider( int clka_or_clkb, int output, int divider )
{
    const struct clkab_output_map_entry *o = clkab_find_map_entry( clka_or_clkb, output );
    struct ltc695x_device* dev = (clka_or_clkb == ERTM14_OUT_CLKA) ? &board.dev_clka_distr : &board.dev_clkb_distr;

    if(!o)
        return -EINVAL;

    ltc6953_configure_output( dev, o->id_ltc6953, divider, o->invert );

    return 0;
}


static int clkab_enable_output( int clka_or_clkb, int output, int enable )
{
    const struct clkab_output_map_entry *o = clkab_find_map_entry( clka_or_clkb, output );
    struct ltc695x_device* dev = (clka_or_clkb == ERTM14_OUT_CLKA) ? &board.dev_clka_distr : &board.dev_clkb_distr;

    if(!o)
        return -EINVAL;

    ltc6953_enable_output( dev, o->id_ltc6953, enable );

    return 0;
}


int ertm14_init_clkab_distribution(void)
{
    /* initialize the SPI bus for the CLKA fanout (LTC6953) */
    bb_spi_create( &board.spi_ltc6953_clka,
        &pin_ertm15_clka_cs_n,
        &pin_ertm15_clkab_mosi,
        &pin_ertm15_clkab_miso,
        &pin_ertm15_clkab_sck,
        1000 );

    ltc695x_init(&board.dev_clka_distr, &board.spi_ltc6953_clka);

    /* initialize the SPI bus for the CLKA fanout (LTC6953) */
    bb_spi_create( &board.spi_ltc6953_clkb,
        &pin_ertm15_clkb_cs_n,
        &pin_ertm15_clkab_mosi,
        &pin_ertm15_clkab_miso,
        &pin_ertm15_clkab_sck,
        1000 );

    ltc695x_init(&board.dev_clkb_distr, &board.spi_ltc6953_clkb);

#define LTC6953_EXPECTED_ID 0x23

    int id_a, id_b;
    id_a = ltc695x_read(&board.dev_clka_distr, 0x38);
    id_b = ltc695x_read(&board.dev_clkb_distr, 0x38);

    int result_a = ltc695x_configure( &board.dev_clka_distr, &clkab_ertm15_bootstrap_config );
    int result_b = ltc695x_configure( &board.dev_clkb_distr, &clkab_ertm15_bootstrap_config );

    bist_checkpoint( ertm_bist, ERTM14_BIST_CLKA, 0, (id_a == LTC6953_EXPECTED_ID) && !result_a );
    bist_checkpoint( ertm_bist, ERTM14_BIST_CLKB, 0, (id_b == LTC6953_EXPECTED_ID) && !result_b );

    if( id_a != LTC6953_EXPECTED_ID || id_b != LTC6953_EXPECTED_ID )
        return -ENODEV;


// set 250 MHz output on CLKA/CLKB on the front panel
    clkab_set_output_divider( ERTM14_OUT_CLKA, ERTM14_CLKAB_OUT_FRONT_PANEL, 4 ); // divide by 4 -> 250 MHz
    clkab_set_output_divider( ERTM14_OUT_CLKB, ERTM14_CLKAB_OUT_FRONT_PANEL, 4 );

    clkab_enable_output( ERTM14_OUT_CLKA, ERTM14_CLKAB_OUT_FRONT_PANEL, 1 );
    clkab_enable_output( ERTM14_OUT_CLKB, ERTM14_CLKAB_OUT_FRONT_PANEL, 1 );

// force a SYNC pulse to make sure the SYNC_N pins of the AD9520s are high
// (so that any clock output is possible)
    fine_pulse_gen_force_pulse( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKA );
    fine_pulse_gen_force_pulse( &board.dds_sync_dev, ERTM14_PLL_SYNC_CLKB );
    return 0;
}

int ertm14_init_ref_clock_distribution(void)
{
    int main_stat = ad951x_init(&board.ad9516_main, &board.spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock);
    int ext_stat = ad951x_init(&board.ad9516_ext, &board.spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock);

    bist_checkpoint( ertm_bist, ERTM14_BIST_AD951X_MAIN, 0, main_stat == 0 );
    bist_checkpoint( ertm_bist, ERTM14_BIST_AD951X_EXT, 0, ext_stat == 0 );

    if( main_stat < 0 )
    {
        board_dbg( "Failed to configure the main clock distribution AD9516 (chip not responding)\n ");
        return -1;
    }

    if( ext_stat < 0 )
    {
        board_dbg( "Failed to configure the external 10 MHz clock multiplier AD9516 (chip not responding)\n ");
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
    return 0;
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

    int probe_ref = ad9910_probe( &board.dds_ad9910_ref, &board.spi_ad9910_ref, ertm14_dds_trigger_ioupdate );
    int probe_lo = ad9910_probe( &board.dds_ad9910_lo, &board.spi_ad9910_lo, ertm14_dds_trigger_ioupdate );

    bist_checkpoint( ertm_bist, ERTM14_BIST_DDS_LO, 0, probe_lo == 0 );
    bist_checkpoint( ertm_bist, ERTM14_BIST_DDS_REF, 0, probe_ref == 0 );
    return 0;
}

int ertm14_init_mac_eeprom(void)
{
    bb_i2c_create( &board.i2c_mac_addr, &pin_mac_addr_scl, &pin_mac_addr_sda );
    bb_i2c_init( &board.i2c_mac_addr );

    m24aa025_init( &board.m24_mac_ids[0], &board.i2c_mac_addr, 0x50 );
    m24aa025_init( &board.m24_mac_ids[1], &board.i2c_mac_addr, 0x51 );

    uint8_t mac[6];

    int err = m24aa025_read_mac( &board.m24_mac_ids[0], mac );
    //m24aa025_read_mac( &board.m24_mac_ids[1], mac );

    bist_checkpoint( ertm_bist, ERTM14_BIST_MAC_EEPROM, 0, !err );

    if( err < 0 )
        return err;

    board_dbg("MAC address: Port 0 = %02x:%02x:%02x:%02x:%02x:%02x\n",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5] );
    ep_set_mac_addr( &wrc_endpoint_dev, mac );

    return 0;
}


void ertm14_set_pps_out_mode(int mode)
{
    gen_gpio_out( &pin_pps_out_mode0, (mode & 0x1) ? 1 : 0);
    gen_gpio_out( &pin_pps_out_mode1, (mode & 0x2) ? 1 : 0);
    gen_gpio_out( &pin_pps_out_mode2, (mode & 0x4) ? 1 : 0);
}

int ertm14_low_level_init(void)
{
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

    x595_gpio_create ( &board.gpio_ertm15_leds, 1, &pin_ertm15_leds_updtclk, &pin_ertm15_leds_shftclk, NULL, &pin_ertm15_leds_ser);

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

    ertm14_test_leds();
    ertm15_test_leds();

    // fixme: detect fail
    //ertm15_check_oscillators();

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

    board_dbg("Init Fine Pulse Generator\n");

    /* Initialize the Fine Pulse Generator - it MUST be done 
       before we touch the DDSes as it drives the DDS IOUPDATE line.
       For my own record: don't touch this, you've wasted time catching the null pointer to
       FPG device already ;-) */

    fine_pulse_gen_create( &board.dds_sync_dev, BASE_ERTM14_DDS_SYNC_UNIT );

    if( ! (board.mode & ERTM14_MODE_WITHOUT_ERTM15 ) )
    {
        board_dbg("Initializing RF distribution\n");

        /* RF Power Monitor ADC (eRTM15 - IC43) */
        bb_spi_create( &board.spi_ad7888,
            &pin_pwrmon_adc_cs_n,
            &pin_pwrmon_adc_din,
            &pin_pwrmon_adc_dout,
            &pin_pwrmon_adc_sclk,
            100 );

        ad7888_create( &board.pwrmon_adc, &board.spi_ad7888 );


        /* RF distribution switches and shift registers controlling these (eRTM15 - IC26..28) */
        ertm15_rf_distr_init( &board.rf_distr, &board.pwrmon_adc );

        /* Now that the PLL clocks are ready, init the DDS synthesizers */
        board_dbg("Initializing DDSes\n");
        ertm15_init_dds();

        /* Program the DDSes to some meaninfgul settings, say, 205 MHz */
        ad9910_program(&board.dds_ad9910_ref, ERTM14_DDS_DEFAULT_FTW, 0, ERTM14_DDS_DEFAULT_AMPLITUDE );
        ad9910_program(&board.dds_ad9910_lo, ERTM14_DDS_DEFAULT_FTW, 0, ERTM14_DDS_DEFAULT_AMPLITUDE );

        ertm15_rf_distr_measure_power ( &board.rf_distr );

        int i;

        board_dbg("PA PWR REF = %d mBm, LO = %d mBm\n", board.rf_distr.pwr_ref_in, board.rf_distr.pwr_lo_in );

        for(i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
        {
            board_dbg("OUT[%d] PWR REF = %d mBm, LO = %d mBm\n", i, board.rf_distr.pwr_ref_ch[i], board.rf_distr.pwr_lo_ch[i] );
        }

        for(;;);

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

    board_dbg("Init Control UART Link\n");
    uart_link_create_wrpc_console( &board.control_uart_link );

    board_dbg("Init MMC14 UART Link\n");
    suart_init( &board.mmc_14_uart, BASE_MMC_UART_14, 115200 );
    uart_link_create_wrpc_suart( &board.mmc_14_link, &board.mmc_14_uart );

    board_dbg("Init RF transceiver\n");
    wr_rf_frame_transceiver_create( &board.rf_xcvr, BASE_ERTM14_RF_FRAME_TRANSCEIVER );

    board_dbg("eRTM14/15 early init done\n");

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
        cfg->lo.ftw = ERTM14_DDS_DEFAULT_FTW;
        cfg->ref.ftw = ERTM14_DDS_DEFAULT_FTW;
        cfg->lo.ampl_factor = ERTM14_DDS_DEFAULT_AMPLITUDE;
        cfg->ref.ampl_factor = ERTM14_DDS_DEFAULT_AMPLITUDE; 

        for( j = 0; j <= ERTM14_RF_OUT_MAX_ID; j++)
        {
            cfg->ref.out_state [j] = ERTM15_RF_OUT_MONITOR;
            cfg->lo.out_state [j] = ERTM15_RF_OUT_MONITOR;
        }
    
        cfg->ref.sync_count = 0;
        cfg->lo.sync_count = 0;

        cfg->ref.sync_source = ERTM14_SYNC_SOURCE_RF_TRIGGER;
        cfg->lo.sync_source = ERTM14_SYNC_SOURCE_RF_TRIGGER;

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
    board_dbg("Apply_config: %d\n", config_id );
    ertm14_current_state = &ertm14_configs[config_id];
    event_post ( WRC_ERTM14_EVENT_APPLY_NEW_CONFIG );
    return 0;
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

            board_dbg("CLKA%d: freq=%d Hz, divider=%d, enable=%d\n", i, freq_a, div_a, enable_a);
            board_dbg("CLKA%d: freq=%d Hz, divider=%d, enable=%d\n", i, freq_b, div_b, enable_b);

            clkab_set_output_divider( ERTM14_OUT_CLKA, i, div_a );
            clkab_set_output_divider( ERTM14_OUT_CLKB, i, div_b );
            clkab_enable_output( ERTM14_OUT_CLKA, i, enable_a );
            clkab_enable_output( ERTM14_OUT_CLKB, i, enable_b );
        }

            // DDSes

        ad9910_program(&board.dds_ad9910_lo, cfg->lo.ftw, 0, cfg->lo.ampl_factor );
        ad9910_program(&board.dds_ad9910_ref, cfg->ref.ftw, 0, cfg->ref.ampl_factor );

        board_dbg("DDS LO: FTW=0x%08x, ampl=%d\n", cfg->lo.ftw, cfg->lo.ampl_factor );
        board_dbg("DDS REF: FTW=0x%08x, ampl=%d\n", cfg->ref.ftw, cfg->ref.ampl_factor );
        
        for( i = ERTM14_RF_OUT_MIN_ID; i <= ERTM14_RF_OUT_MAX_ID; i++)
        {
            int st_lo = cfg->lo.out_state[i] == ERTM15_RF_OUT_ON ? 1 : 0;
            int st_ref = cfg->ref.out_state[i] == ERTM15_RF_OUT_ON ? 1 : 0;
            board_dbg("i %d lo %x ref %x\n", i, st_lo, st_ref );

            ertm15_rf_distr_output_enable( &board.rf_distr, ERTM15_RF_LO, i, st_lo );
            ertm15_rf_distr_output_enable( &board.rf_distr, ERTM15_RF_REF, i, st_ref );
        }

        ertm15_update_rf_switches( &board.rf_distr );
    return 0;
}

static int evth_config_update_listener;

static void ertm14_config_update_init(void)
{

}

static int ertm14_config_update_task(void)
{
    //pp_printf("cutask %d\n", ertm_init_complete );
    if (ertm_init_complete)
    {
        int evt = event_poll( evth_config_update_listener );

        if( evt == WRC_ERTM14_EVENT_APPLY_NEW_CONFIG )
        {
            board_dbg("New config detected, applying...\n");
            ertm14_commit_config(ertm14_current_state);
            event_post( WRC_ERTM14_EVENT_RECONFIGURED );
        }
    }
    return 0;
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


#define ERTM14_EXPECTED_FLASH_ID 0x00016018

int wrc_board_early_init()
{
    static int32_t flash_entry_points[64];
    int i;

    bist_init( ertm_bist );

    /* initialize SPI flash */
    bb_spi_create( &spi_wrc_flash,
		&pin_sysc_spi_ncs,
		&pin_sysc_spi_mosi,
		&pin_sysc_spi_miso,
		&pin_sysc_spi_sclk, 0 );

	spi_flash_create( &wrc_flash_dev, &spi_wrc_flash, 16384, 0x600000 );

	uint32_t id = spi_flash_read_id( &wrc_flash_dev );

    bist_checkpoint( ertm_bist, ERTM14_BIST_FLASH_PRESENCE, 0, id == ERTM14_EXPECTED_FLASH_ID );

	/* initialize I2C bus */
	bb_i2c_init( &dev_i2c_fmc );

    for(i = 0; i < 32 + 8; i++)
        flash_entry_points[i] = 0x600000 + 0x40000 * i;

    flash_entry_points[i] = -1;

    /* init storage (we use the SPI flash on eRTM14) */
    storage_spiflash_create( &wrc_storage_dev, &wrc_flash_dev );
    wrc_storage_dev.entry_points = &flash_entry_points[0];

    int rv = storage_mount( &wrc_storage_dev );
    bist_checkpoint( ertm_bist, ERTM14_BIST_FLASH_FS_MOUNT, 0, rv == 0 );

    /* reset the networking part of the WRCore and start the WR Endpoint */
   	net_rst();

    ep_init( &wrc_endpoint_dev, (void *) BASE_EP );

	netif_register_device( "wru0", "default", &wrc_endpoint_dev );

	/* Sleep for 1s to make sure WRS v4.2 always realizes that
	 * the link is down */
	timer_delay_ms(200);
	ep_enable( &wrc_endpoint_dev, 1, 1);
	timer_delay_ms(200);

    int ll = ertm14_low_level_init();

    bist_summary( ertm_bist );

    return ll;
}

extern int phy_calibration_poll(void);
extern void phy_calibration_init(void);

timeout_t mmc14_tmo;

void mmc14_link_init(void)
{
    tmo_init( &mmc14_tmo, 1000 );
    return 0;
}

int mmc14_link_poll(void)
{
    if (tmo_expired(&mmc14_tmo))
    {
        tmo_restart( &mmc14_tmo );
        struct uart_packet pkt;

        pkt.ptype = ERTM14_UART_PTYPE_MMC_STATUS_REQ;
        pkt.length = 0;
        uart_link_send( &board.mmc_14_link, &pkt );

        pp_printf("req mmc14\n");
    }
    return 0;
}

int wrc_board_init()
{
    ertm14_shell_init();

    evth_dds_nco_sync = event_listener_create();
    evth_config_update_listener = event_listener_create();

    //wrc_task_create( "iuart14", NULL, iuart_14_poll );
    
    console_set_mode_switch_hook( &console_uart_dev, control_uart_mode_callback );

    wrc_task_create( "control-uart", NULL, control_uart_poll );
    wrc_task_create( "rf-nco-sync", ertm14_dds_nco_sync_init, ertm14_dds_nco_sync_task );
    wrc_task_create( "ertm-config", ertm14_config_update_init, ertm14_config_update_task );
    wrc_task_create( "phy-cal", phy_calibration_init, phy_calibration_poll );
    wrc_task_create( "mmc14", mmc14_link_init, mmc14_link_poll );

    ertm14_apply_config( 0 );

    return 0;
}


int wrc_board_create_tasks()
{

    return 0;
}
