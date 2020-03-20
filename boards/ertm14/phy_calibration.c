#include "board.h"
#include "dev/syscon.h"
#include "dev/endpoint.h"
#include <softpll_ng.h>
#include "storage.h"
#include "util.h"

#include <wrc-task.h>

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

#include "dev/clock_monitor.h"

#ifdef ERTM14_CALIBRATION_DEBUG
    #define cal_dbg(...) pp_printf("[cal-dbg]"__VA_ARGS__)
#else
    #define cal_dbg(...)
#endif

#define DEFAULT_COMMA_POS 0

#define MDIO_DBG1_RESET_TX (1 << 0)
#define MDIO_DBG1_TX_ENABLE (1 << 1)
#define MDIO_DBG1_RX_ENABLE (1 << 2)
#define MDIO_DBG1_RESET_RX (1 << 3)
#define MDIO_DBG1_GTX_QPLL_RESET (1 << 4)
#define MDIO_DBG1_GTX_TXUSRPLL_RESET (1 << 5)
#define MDIO_DBG1_COMMA_TARGET_POS(x) ( ((x) & 0x7f) << 6)

#define MDIO_DBG1_DMTD_SOURCE_TXOUTCLK (1 << 14)
#define MDIO_DBG1_DMTD_SOURCE_RXRECCLK (0 << 14)

#define MDIO_DBG0_GTX_QPLL_LOCKED (1 << 0)
#define MDIO_DBG0_LINK_UP (1 << 1)
#define MDIO_DBG0_LINK_ALIGNED (1 << 2)
#define MDIO_DBG0_RESET_TX_DONE (1 << 3)
#define MDIO_DBG0_GTX_TXUSRPLL_LOCKED (1 << 4)
#define MDIO_DBG0_RESET_RX_DONE (1 << 5)

#define MDIO_DBG1 (19<<2)
#define MDIO_DBG0 (18<<2)

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

struct wrc_port_tx_setup_state
{
    int state;
    int cnt;
    int attempts;
    int cal_saved_phase;
    int cal_saved_phase_valid;
    int cal_file_updated;
    int measured_phase;
    int expected_phase;
    int tollerance;
    int update_cnt;
    int expected_phase_valid;
    timeout_t refresh_timeout;
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

static void tx_fsm_init(struct wrc_port_tx_setup_state *fsm)
{
    fsm->attempts = 0;
    fsm->state = TX_SETUP_STATE_START;
    fsm->expected_phase = 0;
    fsm->expected_phase_valid = 0;
    fsm->tollerance = 300;
    fsm->update_cnt = 0;
    fsm->cal_saved_phase_valid = 0;
    fsm->cal_saved_phase = 0;
    fsm->cal_file_updated = 0;
    fsm->cnt = 0;

    if( !storage_get_calibration_parameter( CAL_PARAM_PHY_TARGET_TX_PHASE, &fsm->cal_saved_phase ) )
    {
        cal_dbg("read tx target phase :%d ps\n", fsm->cal_saved_phase);
        fsm->cal_saved_phase_valid = 1;
    }

    tmo_init(&fsm->spll_lock_timeout, FSM_SPLL_LOCK_TIMEOUT_MS);
}

static int tx_fsm_update()
{
    struct wrc_port_tx_setup_state *fsm = &tx_state;


    //pp_printf("Tics %d st %d\n", timer_get_tics(), fsm->state );
    switch (fsm->state)
    {
    case TX_SETUP_STATE_START:
    {

        if( tmo_expired( &fsm->spll_lock_timeout ) )
        {
            cal_dbg("Can't lock the SoftPLL. This is necessary for PHY calibratoin to continue. Retrying...\n");
            tmo_restart( &fsm->spll_lock_timeout );
        }

        if( spll_check_lock( 0 ) )
        {
            spll_enable_ptracker(0, 0);
            ep_pcs_write(MDIO_DBG1, MDIO_DBG1_RESET_RX | MDIO_DBG1_DMTD_SOURCE_TXOUTCLK);
            fsm->state = TX_SETUP_STATE_RESET_PCS;

            tmo_init( &fsm->refresh_timeout, FSM_DEBUG_REFRESH_PERIOD_MS );
        }
        break;
    }

    case TX_SETUP_STATE_RESET_PCS:
    {
        uint32_t dbg1 =  MDIO_DBG1_RESET_TX | MDIO_DBG1_RESET_RX | MDIO_DBG1_DMTD_SOURCE_TXOUTCLK | MDIO_DBG1_GTX_QPLL_RESET | MDIO_DBG1_GTX_TXUSRPLL_RESET;
        
        spll_enable_ptracker(0, 0);
        
        // reset the QPLL
        ep_pcs_write(MDIO_DBG1, dbg1 );
        dbg1 &= ~MDIO_DBG1_GTX_QPLL_RESET;
        ep_pcs_write(MDIO_DBG1, dbg1 );

//        pp_printf("Qpll: ");
        // wait for lock
        while( !( ep_pcs_read(MDIO_DBG0) & MDIO_DBG0_GTX_QPLL_LOCKED ) )
            usleep(1);
        
        //pp_printf("QPLL OK\n"); 

        // QPLL ok: un-reset TX path (+ UsrClk PLL)
        dbg1 &= ~MDIO_DBG1_RESET_TX;
        ep_pcs_write(MDIO_DBG1, dbg1 );

        usleep(100);
        dbg1 &= ~MDIO_DBG1_GTX_TXUSRPLL_RESET;
        ep_pcs_write(MDIO_DBG1, dbg1 );

        fsm->state = TX_SETUP_STATE_WAIT_LOCK;
        tmo_init( &fsm->phy_lock_timeout, FSM_PHY_LOCK_TIMEOUT_MS );

        break;
    }

    case TX_SETUP_STATE_WAIT_LOCK:
    {
        uint32_t dbg0 = ep_pcs_read(MDIO_DBG0);
        if (dbg0 & MDIO_DBG0_RESET_TX_DONE)
        {
            fsm->attempts++;
            fsm->state = TX_SETUP_STATE_MEASURE_PHASE;
            usleep(10000);
            spll_set_ptracker_average_samples( 0, 10 );
            spll_enable_ptracker(0, 1);
            tmo_init( &fsm->dmtd_timeout, FSM_DMTD_TIMEOUT_MS );
        }
        else if( tmo_expired(&fsm->phy_lock_timeout) )
        {
            fsm->state = TX_SETUP_STATE_RESET_PCS;
            cal_dbg("PHY PLL lock timeout, retrying...[ dbg0 %04x]\n", dbg0 );
        }
        break;
    }

    case TX_SETUP_STATE_MEASURE_PHASE:
    {
        int phase, enabled; //, p2;
        int rv = spll_read_ptracker(0, &phase, &enabled);


        if (!rv)
        {
            if( tmo_expired( &fsm->dmtd_timeout ) )
            {
                cal_dbg("Phase measurement timeout, retrying...\n");
                //for(;;)
                  //  spll_show_stats();
                fsm->state = TX_SETUP_STATE_RESET_PCS;
            }
            return 0;
        }

//        p2 = fsm->measured_phase = phase;
        cal_dbg("samples %d last-phase %d\n", fsm->attempts, phase );

        if(tmo_expired(&fsm->refresh_timeout))
        {
//            pp_printf("[tx-cal] samples %d last-phase %d\n", fsm->attempts, fsm->measured_phase);
            tmo_restart(&fsm->refresh_timeout);
        }

        
        if (!fsm->expected_phase_valid)
        {
            if (fsm->cal_saved_phase_valid)
            {
                //pr_info("Using phase from file :%d\n",
                //	fsm->cal_saved_phase);
                fsm->expected_phase = fsm->cal_saved_phase;
                fsm->tollerance = 150; /*ps, bins are 200 ps wide*/
            }
            else // find a sane default
            {
                fsm->expected_phase = 10;
                fsm->tollerance = 350; // fixme: this works for PHY oversampling at 5 Gbps (must be made generic at some time...)
            }
            fsm->expected_phase_valid = 1;
        }

        
        int phase_min = fsm->expected_phase - fsm->tollerance;
        int phase_max = fsm->expected_phase + fsm->tollerance;



        if (within_range(phase, phase_min, phase_max, 16000))
        {
            int i;

            fsm->measured_phase = phase;
            cal_dbg("FIX phase %d\n", fsm->measured_phase );

            //spll_enable_ptracker(0, 0);
            //spll_set_ptracker_average_samples( PTRACKER_AVERAGE_SAMPLES );
            //spll_enable_ptracker(0, 1);

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
        int phase, enabled;
        //int rv = spll_read_ptracker(0, &phase, &enabled);

        //if (!rv)
          //  return 0;

        //fsm->measured_phase = phase;
        cal_dbg("TX calibration complete (phase %d ps)\n", fsm->measured_phase);
        spll_enable_ptracker(0, 0);

        // enable the PCS on the port
        ep_pcs_write(MDIO_DBG1, MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK);

        if( !fsm->cal_saved_phase_valid )
        {
            cal_dbg("saving new target phase: %d ps\n", fsm->measured_phase);
            storage_set_calibration_parameter( CAL_PARAM_PHY_TARGET_TX_PHASE, fsm->measured_phase);
            storage_save_calibration();
        }

        fsm->state = TX_SETUP_DONE;
        break;
    }

    case TX_SETUP_DONE:
    {
    	int early_link_up = ep_pcs_read(MDIO_DBG0) & MDIO_DBG0_LINK_UP;

        return 1;
        break;
    }
    }

    return 0;
}


static void rx_fsm_init(  )
{
	struct wrc_port_rx_setup_state* fsm = &rx_state;

	fsm->attempts = 0;
	fsm->state = RX_SETUP_STATE_INIT;
    fsm->prev_link_up = 0;
    memset(fsm->cpos_stat, 0, sizeof(fsm->cpos_stat ));
}

static int rx_fsm_update(  )
{
	struct wrc_port_rx_setup_state* fsm = &rx_state;
	struct wrc_port_tx_setup_state* fsm_tx = &tx_state;

	int early_link_up = ep_pcs_read(MDIO_DBG0) & MDIO_DBG0_LINK_UP;


	if( fsm_tx->state != TX_SETUP_DONE )
	{
		fsm->state = RX_SETUP_STATE_INIT;
		return 0;
	}

    	switch( fsm->state )
	{
		case RX_SETUP_STATE_INIT:
		{
			ep_pcs_write( MDIO_DBG1, MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK | MDIO_DBG1_COMMA_TARGET_POS(DEFAULT_COMMA_POS) );
			
			if (early_link_up) {
				if ( fsm_tx->state == TX_SETUP_DONE )
				{
					cal_dbg("RX calibration started.\n");
	
					fsm->state = RX_SETUP_STATE_RESET_PCS;
				}
			}

			fsm->attempts = 0;

			break;
		}

		case RX_SETUP_STATE_RESET_PCS:
		{
			if (early_link_up) 
            {
				fsm->state = RX_SETUP_STATE_WAIT_LOCK;

				ep_pcs_write( MDIO_DBG1, MDIO_DBG1_RESET_RX | MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK | MDIO_DBG1_COMMA_TARGET_POS(DEFAULT_COMMA_POS)  );
				usleep(1);
				ep_pcs_write( MDIO_DBG1, MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK | MDIO_DBG1_COMMA_TARGET_POS(DEFAULT_COMMA_POS)  );
                usleep(10000);
				fsm->attempts++;

                tmo_init(&fsm->link_timeout, FSM_EARLY_LINK_UP_TIMEOUT_MS);
			}

			break;
		}

		case RX_SETUP_STATE_WAIT_LOCK:
		{
			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);

			int rx_up = dbg0 & MDIO_DBG0_LINK_UP;
			int rx_aligned = dbg0 & MDIO_DBG0_LINK_ALIGNED;
   			int rx_comma_pos = (dbg0 >> 7) & 0x7f;
            int rx_comma_valid = (dbg0 >> 7) & 0x80 ? 1 : 0;


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
                    
                    # if 0
                       int i;

                    {
                        	dbg0 = ep_pcs_read( MDIO_DBG0);

			                rx_up = dbg0 & MDIO_DBG0_LINK_UP;
			                rx_aligned = dbg0 & MDIO_DBG0_LINK_ALIGNED;
            
   			rx_comma_pos = (dbg0 >> 7) & 0x7f;
            rx_comma_valid = (dbg0 >> 7) & 0x80 ? 1 : 0;
                        pp_printf("Dbg0 %x up %d algn %d cpos %d cvalid %d\n", dbg0, rx_up, rx_aligned, rx_comma_pos, rx_comma_valid );
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
                
			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);


            int rx_up = dbg0 & MDIO_DBG0_LINK_UP;
			int rx_aligned = dbg0 & MDIO_DBG0_LINK_ALIGNED;
   			int rx_comma_pos = (dbg0 >> 7) & 0x7f;
            int rx_comma_valid = (dbg0 >> 7) & 0x80 ? 1 : 0;

			if ( rx_up && rx_aligned && rx_comma_valid && (rx_comma_pos == DEFAULT_COMMA_POS) )
			{
    			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);
       			int rx_comma_pos = (dbg0 >> 7) & 0x7f;
				ep_pcs_write( MDIO_DBG1, MDIO_DBG1_RX_ENABLE | MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK | MDIO_DBG1_COMMA_TARGET_POS(DEFAULT_COMMA_POS) );
				ep_pcs_write( MDIO_REG_MCR, MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK | MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART  );
				cal_dbg("RX calibration complete (after %d attempts) comma @ %d taps.\n", fsm->attempts, rx_comma_pos );
				spll_enable_ptracker( 0, 0 );
                spll_set_ptracker_average_samples( 0, PTRACKER_AVERAGE_SAMPLES );

       			fsm->state = RX_SETUP_DONE;

			} else {
                cal_dbg("weird, can't stabilize link. Retrying [%d %d %d %d]\n", rx_up, rx_aligned, rx_comma_valid, rx_comma_pos );
                fsm->state = RX_SETUP_STATE_RESET_PCS;
            }

			break;
		}



		case RX_SETUP_DONE:
		{
			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);
            int link_up = ep_link_up(NULL);


			if( ! (dbg0 & MDIO_DBG0_LINK_UP ) /*|| ( ( fsm->prev_link_up && !link_up ) )*/ )
			{
				cal_dbg("port went down, need RX recalibration.\n");
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

int phy_calibration_poll()
{
    tx_fsm_update();
    rx_fsm_update();
    return 0;
}



void phy_calibration_init()
{
    cal_dbg("Initializing PHY calibrator...\n");
    ep_pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN);	/* reset the PHY */
	timer_delay_ms(200);
	ep_pcs_write(MDIO_REG_MCR, MDIO_MCR_RESET);	/* reset the PHY */
	ep_pcs_write(MDIO_REG_MCR, 0);	/* reset the PHY */

 	spll_init( SPLL_MODE_FREE_RUNNING_MASTER, 0, 0 );
    spll_set_ptracker_average_samples( 0, 10 );

    tx_fsm_init(&tx_state);
    rx_fsm_init(&rx_state);
}

void phy_calibration_disable()
{
    tx_state.state = TX_SETUP_STATE_DISABLED;
    rx_state.state = RX_SETUP_STATE_DISABLED;
}