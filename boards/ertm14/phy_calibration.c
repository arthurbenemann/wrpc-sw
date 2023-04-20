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

/*
    LPDC PHY Calibration code.

    Specific for Kintex-7 devices on the WR2RF-VME and eRTM14/15 cards ONLY!
*/


#include <string.h>
#include <board.h>
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include <softpll_ng.h>
#include "storage.h"
#include "util.h"
#include "wrc-debug.h"
#include "wrc-task.h"

#include <hw/ep_mdio_regs.h>
#include <hw/lpdc_mdio_regs.h>

#include "dev/clock_monitor.h"

/* middle of clock cycle seems the safest, we observed glitches around 15000->1000 ps */
#define LPDC_COARSE_PHASE_MIN_PS 8500    /* ps */
#define LPDC_COARSE_PHASE_MAX_PS 9000    /* ps */
#define LPDC_FINE_PHASE_TOLLERANCE_PS 40 /* ps */

#define DEFAULT_COMMA_POS 0

#define LPDC_MDIO_CTRL_DMTD_SOURCE_TXOUTCLK (1 << LPDC_MDIO_CTRL_DMTD_CLK_SEL_SHIFT)
#define LPDC_MDIO_CTRL_DMTD_SOURCE_RXRECCLK (0 << LPDC_MDIO_CTRL_DMTD_CLK_SEL_SHIFT)

#define TX_SETUP_STATE_START 0
#define TX_SETUP_STATE_RESET_PCS 1
#define TX_SETUP_STATE_WAIT_LOCK 2
#define TX_SETUP_STATE_MEASURE_PHASE 3
#define TX_SETUP_DONE 4
#define TX_SETUP_VALIDATE 5
#define TX_SETUP_STATE_DISABLED 6

#define RX_SETUP_STATE_INIT 0
#define RX_SETUP_STATE_RESET_PCS 1
#define RX_SETUP_STATE_WAIT_LOCK 2
#define RX_SETUP_STATE_MEASURE_PHASE 3
#define RX_SETUP_DONE 4
#define RX_SETUP_VALIDATE 5
#define RX_SETUP_STATE_DISABLED 6

#define FSM_DEBUG_REFRESH_PERIOD_MS 1000
#define FSM_PHY_LOCK_TIMEOUT_MS 1000
#define FSM_SPLL_LOCK_TIMEOUT_MS 10000
#define FSM_DMTD_TIMEOUT_MS 100
#define FSM_EARLY_LINK_UP_TIMEOUT_MS 100
#define FSM_STABILIZE_TIMEOUT_MS 100

#define LPDC_EXTRA_DEBUG

struct wrc_port_tx_setup_state
{
    int state;
    int cnt;
    int cal_saved_phase;
    int cal_saved_phase_valid;
    int cal_file_updated;
    int measured_phase;
    int expected_phase;
    int tollerance;
    int update_cnt;
    int expected_phase_valid;
    timeout_t phy_lock_timeout;
    timeout_t spll_lock_timeout;
    timeout_t dmtd_timeout;
};

struct wrc_port_rx_setup_state
{
	int cpos_stat[20];
	int state;
	int attempts;
    int prev_link_up;
    timeout_t link_timeout;
    timeout_t stabilize_timeout;
};

static struct wrc_port_tx_setup_state tx_state;
static struct wrc_port_rx_setup_state rx_state;

static inline void mdio_lpdc_write(struct wr_endpoint_device *dev, int location, uint16_t value)
{
    ep_pcs_write(&wrc_endpoint_dev,  
    location + EP_MDIO_PHY_SPECIFIC_REGS,
    value);
}

static inline uint16_t mdio_lpdc_read(struct wr_endpoint_device *dev, int location)
{
    return ep_pcs_read(&wrc_endpoint_dev,  
    location + EP_MDIO_PHY_SPECIFIC_REGS);
}

static inline void mdio_xdrp_write(struct wr_endpoint_device *dev, int location, uint16_t value)
{
    ep_pcs_write(&wrc_endpoint_dev,
    location + EP_MDIO_PHY_SPECIFIC_REGS + LPDC_MDIO_DRP_REGS,
    value);
}

static inline uint16_t mdio_xdrp_read(struct wr_endpoint_device *dev, int location)
{
    return ep_pcs_read(&wrc_endpoint_dev,  
    location + EP_MDIO_PHY_SPECIFIC_REGS + LPDC_MDIO_DRP_REGS);
}


void dump_xdrp_regs(struct wr_endpoint_device *dev)
{
    phy_dbg("[lpdc] XDRP regs dump:\n");
    int i;
    for(i=0;i<128;i++)
        phy_dbg("     R%d = %04x\n", i, mdio_xdrp_read(dev, i*4));
}

static void tx_fsm_init(struct wrc_port_tx_setup_state *fsm)
{
    fsm->state = TX_SETUP_STATE_START;
    fsm->expected_phase = 0;
    fsm->expected_phase_valid = 0;
    fsm->tollerance = 300;
    fsm->update_cnt = 0;
    fsm->cal_saved_phase_valid = 0;
    fsm->cal_saved_phase = 0;
    fsm->cal_file_updated = 0;
    fsm->cnt = 0;

    /* FIXME: is cal_saved_phase unsigned? uint32_t? declare it so
     * at the wrc_port_tx_setup_state structure */
    if( !storage_get_calibration_parameter( CAL_PARAM_PHY_TARGET_TX_PHASE, (uint32_t *)&fsm->cal_saved_phase ) )
    {
        phy_dbg("[lpdc] TX target phase from calibration data: %d ps\n", fsm->cal_saved_phase);
        fsm->cal_saved_phase_valid = 1;
    }

    tmo_init(&fsm->spll_lock_timeout, FSM_SPLL_LOCK_TIMEOUT_MS);
}

static int tx_fsm_update(void)
{
    struct wrc_port_tx_setup_state *fsm = &tx_state;

    switch (fsm->state)
    {
    case TX_SETUP_STATE_START:
    {

        if( tmo_expired( &fsm->spll_lock_timeout ) )
        {
            phy_dbg("[lpdc] can't lock the SoftPLL. This is necessary for PHY calibration to continue. Retrying...\n");
            tmo_restart( &fsm->spll_lock_timeout );
        }

        if( spll_check_lock( 0 ) )
        {
            spll_enable_ptracker(0, 0);
            mdio_lpdc_write( &wrc_endpoint_dev, LPDC_MDIO_CTRL, LPDC_MDIO_CTRL_RX_SW_RESET | LPDC_MDIO_CTRL_DMTD_SOURCE_TXOUTCLK);
            fsm->state = TX_SETUP_STATE_RESET_PCS;
            phy_dbg("[lpdc] SPLL locked\n");

        }

        //dump_xdrp_regs( &wrc_endpoint_dev );
        break;
    }

    case TX_SETUP_STATE_RESET_PCS:
    {
        uint32_t lpc_ctrl =  LPDC_MDIO_CTRL_TX_SW_RESET | LPDC_MDIO_CTRL_RX_SW_RESET | LPDC_MDIO_CTRL_DMTD_SOURCE_TXOUTCLK | LPDC_MDIO_CTRL_QPLL_SW_RESET | LPDC_MDIO_CTRL_TXUSRPLL_RESET;
        phy_dbg("[lpdc] tx reset pcs\n");

        spll_enable_ptracker(0, 0);
        
        // reset the QPLL
        mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, lpc_ctrl );
        lpc_ctrl &= ~LPDC_MDIO_CTRL_QPLL_SW_RESET;
        mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, lpc_ctrl );

        // wait for lock
        int lock_cycles=0;
        while( !( mdio_lpdc_read(&wrc_endpoint_dev, LPDC_MDIO_STAT) & LPDC_MDIO_STAT_QPLL_LOCKED ) )
            lock_cycles++;
        



        // QPLL ok: un-reset TX path (+ UsrClk PLL)
        lpc_ctrl &= ~LPDC_MDIO_CTRL_TX_SW_RESET;
        mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, lpc_ctrl );

        usleep(100);
        lpc_ctrl &= ~LPDC_MDIO_CTRL_TXUSRPLL_RESET;
        mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, lpc_ctrl );

        fsm->state = TX_SETUP_STATE_WAIT_LOCK;
        tmo_init( &fsm->phy_lock_timeout, FSM_PHY_LOCK_TIMEOUT_MS );

        break;
    }

    case TX_SETUP_STATE_WAIT_LOCK:
    {
        uint32_t lpc_stat = mdio_lpdc_read(&wrc_endpoint_dev, LPDC_MDIO_STAT);
        if (lpc_stat & LPDC_MDIO_STAT_TX_RST_DONE)
        {
            fsm->state = TX_SETUP_STATE_MEASURE_PHASE;
            usleep(10000);
            spll_set_ptracker_average_samples( 0, 10 );
            spll_enable_ptracker(0, 1);
            tmo_init( &fsm->dmtd_timeout, FSM_DMTD_TIMEOUT_MS );
        }
        else if( tmo_expired(&fsm->phy_lock_timeout) )
        {
            fsm->state = TX_SETUP_STATE_RESET_PCS;
            phy_dbg("PHY PLL lock timeout, retrying...[ lpc_stat %04x]\n", lpc_stat );
        }
        break;
    }

    case TX_SETUP_STATE_MEASURE_PHASE:
    {
        int32_t phase;
        int enabled; //, p2;
        int rv = spll_read_ptracker(0, &phase, &enabled);


        if (!rv)
        {
            if( tmo_expired( &fsm->dmtd_timeout ) )
            {
                phy_dbg("[lpdc] Phase measurement timeout, retrying...\n");
                fsm->state = TX_SETUP_STATE_RESET_PCS;
            }
            return 0;
        }

#ifdef LPDC_EXTRA_DEBUG
        phy_dbg("[lpdc] TX phase = %d ps, expected = %d, tollerance = %d\n", phase, fsm->expected_phase, fsm->tollerance );
#endif

        if (!fsm->expected_phase_valid)
        {
            if (0) //fsm->cal_saved_phase_valid )
            {
                if ( within_range( fsm->cal_saved_phase, LPDC_COARSE_PHASE_MIN_PS, LPDC_COARSE_PHASE_MAX_PS, 16000 ) )
                {
                    fsm->expected_phase = fsm->cal_saved_phase;
                    fsm->tollerance = LPDC_FINE_PHASE_TOLLERANCE_PS;

                    phy_dbg("[lpdc] Using the previous phase setpoint as the target with tollerance = %d ps\n", fsm->tollerance );
                //	fsm->cal_saved_phase);
                } else {
                    fsm->expected_phase = (LPDC_COARSE_PHASE_MAX_PS + LPDC_COARSE_PHASE_MIN_PS) / 2;
                    fsm->tollerance = (LPDC_COARSE_PHASE_MAX_PS - LPDC_COARSE_PHASE_MIN_PS) / 2;
                    fsm->cal_saved_phase_valid = 0;
                    phy_dbg("[lpdc] Previous phase setpoint (%d ps) out of range. Old calibration algorithm? Restarting from scratch.\n", fsm->cal_saved_phase );
                }
            }
            else // find a sane default
            {
                fsm->expected_phase = (LPDC_COARSE_PHASE_MAX_PS + LPDC_COARSE_PHASE_MIN_PS) / 2;
                fsm->tollerance = (LPDC_COARSE_PHASE_MAX_PS - LPDC_COARSE_PHASE_MIN_PS) / 2;
                phy_dbg("[lpdc] No LPDC TX Calibration data found. Restarting from scratch.\n" );
            }
            fsm->expected_phase_valid = 1;
        }

        
        int phase_min = fsm->expected_phase - fsm->tollerance;
        int phase_max = fsm->expected_phase + fsm->tollerance;

        if (within_range(phase, phase_min, phase_max, 16000))
        {
            fsm->measured_phase = phase;
            phy_dbg("[lpdc] Fix phase = %d ps\n", fsm->measured_phase );
            fsm->state = TX_SETUP_VALIDATE;
        }
        else
        {
            fsm->state = TX_SETUP_STATE_RESET_PCS;
        }

        break;
    }

    case TX_SETUP_VALIDATE:
    {
        //int phase, enabled;
        //int rv = spll_read_ptracker(0, &phase, &enabled);

        //if (!rv)
          //  return 0;

        //fsm->measured_phase = phase;
        phy_dbg("[lpdc] TX calibration complete (phase %d ps)\n", fsm->measured_phase);
        spll_enable_ptracker(0, 0);

        // enable the PCS on the port
        mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, LPDC_MDIO_CTRL_TX_ENABLE | LPDC_MDIO_CTRL_DMTD_SOURCE_RXRECCLK);
        ep_pcs_write(&wrc_endpoint_dev, EP_MDIO_WR_SPEC, 0);

        if( !fsm->cal_saved_phase_valid )
        {
            phy_dbg("[lpdc] Saving established target phase as calibration parameter: %d ps\n", fsm->measured_phase);
            storage_set_calibration_parameter_and_save( CAL_PARAM_PHY_TARGET_TX_PHASE, fsm->measured_phase);
        }

        fsm->state = TX_SETUP_DONE;
        break;
    }

    case TX_SETUP_DONE:
    {
        return 1;
        break;
    }
    }

    return 0;
}


static void rx_fsm_init(struct wrc_port_rx_setup_state* fsm)
{
	fsm->attempts = 0;
	fsm->state = RX_SETUP_STATE_INIT;
    fsm->prev_link_up = 0;
    memset(fsm->cpos_stat, 0, sizeof(fsm->cpos_stat ));
}

static int rx_fsm_update(void)
{
	struct wrc_port_rx_setup_state* fsm = &rx_state;
	struct wrc_port_tx_setup_state* fsm_tx = &tx_state;

    unsigned lpc_stat = mdio_lpdc_read(&wrc_endpoint_dev, LPDC_MDIO_STAT);
    int rx_up =  lpc_stat & LPDC_MDIO_STAT_LINK_UP;

    const uint32_t ctrl_default = LPDC_MDIO_CTRL_TX_ENABLE
         | LPDC_MDIO_CTRL_DMTD_SOURCE_RXRECCLK 
         | (DEFAULT_COMMA_POS << LPDC_MDIO_CTRL_COMMA_TARGET_POS_SHIFT);


	if( fsm_tx->state != TX_SETUP_DONE )
	{
		fsm->state = RX_SETUP_STATE_INIT;
		return 0;
	}

    	switch( fsm->state )
	{
		case RX_SETUP_STATE_INIT:
		{
	mdio_lpdc_write(&wrc_endpoint_dev,  LPDC_MDIO_CTRL, ctrl_default );

	if (rx_up) {
				if ( fsm_tx->state == TX_SETUP_DONE )
				{
					phy_dbg("[lpdc] RX calibration started.\n");
	
					fsm->state = RX_SETUP_STATE_RESET_PCS;
				}
			}

			fsm->attempts = 0;

			break;
		}

		case RX_SETUP_STATE_RESET_PCS:
		{
	if (rx_up)
            {
	    
        	mdio_lpdc_write(&wrc_endpoint_dev,  LPDC_MDIO_CTRL, ctrl_default );

            const unsigned ctrl = ctrl_default;

				fsm->state = RX_SETUP_STATE_WAIT_LOCK;

	    mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL,
			 LPDC_MDIO_CTRL_RX_SW_RESET | ctrl);
				usleep(1);
	    mdio_lpdc_write(&wrc_endpoint_dev, LPDC_MDIO_CTRL, ctrl);

                usleep(10000);
				fsm->attempts++;

                tmo_init(&fsm->link_timeout, FSM_EARLY_LINK_UP_TIMEOUT_MS);
			}

			break;
		}

		case RX_SETUP_STATE_WAIT_LOCK:
		{
	int rx_aligned = lpc_stat & LPDC_MDIO_STAT_LINK_ALIGNED;

			if ( tmo_expired(&fsm->link_timeout) && !rx_up) {
				fsm->state = RX_SETUP_STATE_INIT;
            }
            else 
            {
				if ( !rx_up )
					return 0;

//                fsm->cpos_stat[rx_comma_pos]++;
                
                if( rx_aligned )
				{
                    
                    #ifdef LPDC_EXTRA_DEBUG
                       
                    {
		    lpc_stat = mdio_lpdc_read( &wrc_endpoint_dev, LPDC_MDIO_STAT);

		    rx_up = lpc_stat & LPDC_MDIO_STAT_LINK_UP;
		    rx_aligned = lpc_stat & LPDC_MDIO_STAT_LINK_ALIGNED;
            
		    uint32_t rx_comma_pos = (lpc_stat & LPDC_MDIO_STAT_COMMA_CURRENT_POS_MASK ) >> LPDC_MDIO_STAT_COMMA_CURRENT_POS_SHIFT;
		    pp_printf("Lpc_Stat %x up %d algn %d cpos %lu\n", lpc_stat, rx_up, rx_aligned, rx_comma_pos);
          				usleep(100000);

                    }
                    usleep(100000);

                    #endif

     				fsm->state = RX_SETUP_VALIDATE;
                    tmo_init( &fsm->stabilize_timeout, FSM_STABILIZE_TIMEOUT_MS );
				} else {
					fsm->state = RX_SETUP_STATE_RESET_PCS;
				}
			}
			break;
		}

		case RX_SETUP_VALIDATE:
		{
            if( !tmo_expired( &fsm->stabilize_timeout ))
                return 0;
                
	int rx_aligned = lpc_stat & LPDC_MDIO_STAT_LINK_ALIGNED;
	int rx_comma_pos = (lpc_stat >> 7) & 0x7f;
	int rx_comma_valid = (lpc_stat >> 7) & 0x80 ? 1 : 0;

			if ( rx_up && rx_aligned && rx_comma_valid && (rx_comma_pos == DEFAULT_COMMA_POS) )
			{
	            mdio_lpdc_write(&wrc_endpoint_dev,  
                LPDC_MDIO_CTRL, 
                LPDC_MDIO_CTRL_RX_ENABLE | LPDC_MDIO_CTRL_TX_ENABLE | LPDC_MDIO_CTRL_DMTD_SOURCE_RXRECCLK | ( DEFAULT_COMMA_POS << LPDC_MDIO_CTRL_COMMA_TARGET_POS_SHIFT ) );
				ep_pcs_write(&wrc_endpoint_dev,  EP_MDIO_MCR, EP_MDIO_MCR_SPEED1000 | EP_MDIO_MCR_FULLDPLX | EP_MDIO_MCR_ANENABLE | EP_MDIO_MCR_ANRESTART  );
				phy_dbg("[lpdc] RX calibration complete (after %d attempts) comma @ %d taps.\n", fsm->attempts, rx_comma_pos );
				spll_enable_ptracker( 0, 0 );
                spll_set_ptracker_average_samples( 0, PTRACKER_AVERAGE_SAMPLES );

       			fsm->state = RX_SETUP_DONE;

			} else {
                phy_dbg("[lpdc] Weird, can't stabilize link. Retrying [%d %d %d %d]\n", rx_up, rx_aligned, rx_comma_valid, rx_comma_pos );
                fsm->state = RX_SETUP_STATE_RESET_PCS;
            }

			break;
		}

		case RX_SETUP_DONE:
		{
            int link_up = ep_link_up(&wrc_endpoint_dev, NULL);

	if( !rx_up /*|| ( ( fsm->prev_link_up && !link_up ) )*/ )
			{
				phy_dbg("[lpdc] port went down, need RX recalibration.\n");
				fsm->state = RX_SETUP_STATE_INIT;
                fsm->prev_link_up = link_up;
				return 0;
			}

            fsm->prev_link_up = link_up;
			return 1;
			break;
		}
	}


	return 0;
}

int phy_calibration_poll(void)
{
    tx_fsm_update();
    rx_fsm_update();
    return 1;
}



void phy_calibration_init(void)
{
    phy_dbg("[lpdc] Initializing PHY calibrator...\n");
    ep_pcs_write(&wrc_endpoint_dev, EP_MDIO_MCR, EP_MDIO_MCR_PDOWN);	/* reset the PHY */
	timer_delay_ms(200);
	ep_pcs_write(&wrc_endpoint_dev, EP_MDIO_MCR, EP_MDIO_MCR_RESET);	/* reset the PHY */
	ep_pcs_write(&wrc_endpoint_dev, EP_MDIO_MCR, 0);	/* reset the PHY */

 	spll_init( SPLL_MODE_FREE_RUNNING_MASTER, 0, 0 );
    spll_set_ptracker_average_samples( 0, 10 );

    tx_fsm_init(&tx_state);
    rx_fsm_init(&rx_state);
}

void phy_calibration_disable(void)
{
    tx_state.state = TX_SETUP_STATE_DISABLED;
    rx_state.state = RX_SETUP_STATE_DISABLED;
}
