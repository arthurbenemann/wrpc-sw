#include "board.h"
#include "syscon.h"
#include <endpoint.h>
#include <softpll_ng.h>
#include "storage.h"

#include <wrc-task.h>

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

#include "dev/clock_monitor.h"

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

#define RX_SETUP_STATE_INIT 0
#define RX_SETUP_STATE_RESET_PCS 1
#define RX_SETUP_STATE_WAIT_LOCK 2
#define RX_SETUP_STATE_MEASURE_PHASE 3
#define RX_SETUP_DONE 4
#define RX_SETUP_VALIDATE 5

#define FSM_DEBUG_REFRESH_PERIOD_MS 1000
#define FSM_LOCK_TIMEOUT_MS 1000
#define FSM_DMTD_TIMEOUT_MS 100




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
    timeout_t lock_timeout;
    timeout_t dmtd_timeout;
};

struct wrc_port_rx_setup_state
{
//	timeout_t link_timeout;
    int cpos_stat[20];
	int state;
	int attempts;
    timeout_t dmtd_timeout;
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
}

static int within_range(int x, int minval, int maxval, int wrap)
{
    int rv;

    //printf("min %d max %d x %d ", minval, maxval, x);

    while (maxval >= wrap)
        maxval -= wrap;

    while (maxval < 0)
        maxval += wrap;

    while (minval >= wrap)
        minval -= wrap;

    while (minval < 0)
        minval += wrap;

    while (x < 0)
        x += wrap;

    while (x >= wrap)
        x -= wrap;

    if (maxval > minval)
        rv = (x >= minval && x <= maxval) ? 1 : 0;
    else
        rv = (x >= minval || x <= maxval) ? 1 : 0;

    return rv;
}

static int tx_fsm_update()
{
    struct wrc_port_tx_setup_state *fsm = &tx_state;


    //pp_printf("Tics %d st %d\n", timer_get_tics(), fsm->state );
    switch (fsm->state)
    {
    case TX_SETUP_STATE_START:
    {
        spll_enable_ptracker(0, 0);
        ep_pcs_write(MDIO_DBG1, MDIO_DBG1_RESET_RX | MDIO_DBG1_DMTD_SOURCE_TXOUTCLK);
        fsm->state = TX_SETUP_STATE_RESET_PCS;
        
        tmo_init( &fsm->refresh_timeout, FSM_DEBUG_REFRESH_PERIOD_MS );
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
        tmo_init( &fsm->lock_timeout, FSM_LOCK_TIMEOUT_MS );

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
            spll_set_ptracker_average_samples( 10 );
            spll_enable_ptracker(0, 1);
            tmo_init( &fsm->dmtd_timeout, FSM_DMTD_TIMEOUT_MS );
        }
        else if( tmo_expired(&fsm->lock_timeout) )
        {
            fsm->state = TX_SETUP_STATE_RESET_PCS;
            pp_printf("[tx-cal] PLL lock timeout, retrying...[ dbg0 %04x]\n", dbg0 );
        }
        break;
    }

    case TX_SETUP_STATE_MEASURE_PHASE:
    {
        int phase, enabled, p2;
        int rv = spll_read_ptracker(0, &phase, &enabled);


        if (!rv)
        {
            if( tmo_expired( &fsm->dmtd_timeout ) )
            {
                pp_printf("[tx-cal] Phase measurement timeout, retrying...\n");
                for(;;)
                    spll_show_stats();
                fsm->state = TX_SETUP_STATE_RESET_PCS;
            }
            return 0;
        }

        p2 = fsm->measured_phase = phase;

        if(tmo_expired(&fsm->refresh_timeout))
        {
            pp_printf("[tx-cal] samples %d last-phase %d\n", fsm->attempts, fsm->measured_phase);
            tmo_restart(&fsm->refresh_timeout);
        }

        //pp_printf("Phase: %d\n", phase);
        #if 0
        if (!fsm->expected_phase_valid)
        {
            if (fsm->cal_saved_phase_valid)
            {
                //pr_info("Using phase from file :%d\n",
                //	fsm->cal_saved_phase);
                fsm->expected_phase = fsm->cal_saved_phase;
            }
            else
            {
                int phi = phase;

                do // find the phase bin right after the rising parallel clock edge
                {
                    fsm->expected_phase = phi;
                    phi -= 800;
                } while (phi > 2000);
            }
            fsm->expected_phase_valid = 1;
        }
#endif
        fsm->expected_phase = 10;
        fsm->tollerance = 350;

        int phase_min = fsm->expected_phase - fsm->tollerance;
        int phase_max = fsm->expected_phase + fsm->tollerance;



        if (within_range(phase, phase_min, phase_max, 16000))
        {
            int i;

            fsm->measured_phase = phase;
            //pp_printf("[tx-cal] FIX phase %d\n", fsm->measured_phase );

            spll_enable_ptracker(0, 0);
            spll_set_ptracker_average_samples( PTRACKER_AVERAGE_SAMPLES );
            spll_enable_ptracker(0, 1);

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
        int rv = spll_read_ptracker(0, &phase, &enabled);

        if (!rv)
            return 0;

        fsm->measured_phase = phase;
        pp_printf("[tx-cal] TX calibration complete (phase %d ps)\n", fsm->measured_phase);
        spll_enable_ptracker(0, 0);

        // enable the PCS on the port
        ep_pcs_write(MDIO_DBG1, MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK);

        fsm->state = TX_SETUP_DONE;
        break;
    }

    case TX_SETUP_DONE:
    {
    	int early_link_up = ep_pcs_read(MDIO_DBG0) & MDIO_DBG0_LINK_UP;

        //pp_printf("Early: %d dbg1 0x%04x\n", early_link_up, ep_pcs_read(MDIO_DBG1));
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
					pp_printf("[rx-cal]: calibration started.\n");
	
					fsm->state = RX_SETUP_STATE_RESET_PCS;
					spll_enable_ptracker( 0, 0 );
                    spll_set_ptracker_average_samples( 10 );

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
            int i;

            

			/*if (libwr_tmo_expired(&fsm->link_timeout) && !rx_up) {
				fsm->state = RX_SETUP_STATE_INIT;
			} else {*/
            {
				if ( !rx_up )
					return 0;

                fsm->cpos_stat[rx_comma_pos]++;
                
                //for(i=0;i<20;i++) pp_printf("%-02d: %-03d ", i, fsm->cpos_stat[i]);
                //pp_printf("\n");
				//if( rx_comma_pos == DEFAULT_COMMA_POS )
                  //  pp_printf("Dbg0 %x up %d algn %d cpos %d cvalid %d\n", dbg0, rx_up, rx_aligned, rx_comma_pos, rx_comma_valid );

                if( rx_aligned )
				{
                    
                    # if 0
                       int i;

                    for(i=0;i<20;i++)
                    {
                        	dbg0 = ep_pcs_read( MDIO_DBG0);

			                rx_up = dbg0 & MDIO_DBG0_LINK_UP;
			                rx_aligned = dbg0 & MDIO_DBG0_LINK_ALIGNED;
            
   			rx_comma_pos = (dbg0 >> 7) & 0x7f;
            rx_comma_valid = (dbg0 >> 7) & 0x80 ? 1 : 0;
                        pp_printf("Dbg0 %x up %d algn %d cpos %d cvalid %d\n", dbg0, rx_up, rx_aligned, rx_comma_pos, rx_comma_valid );
          				usleep(100000);

                    }
                    #endif
                    usleep(100000);


					spll_enable_ptracker( 0, 0 );
                    spll_set_ptracker_average_samples( PTRACKER_AVERAGE_SAMPLES );
					spll_enable_ptracker( 0, 1 );
                    usleep(1000000);

              //      pp_printf("Hit, validating\n");
					//tx_fsm_pll_state.channels[p->hw_index].flags = 0;
					fsm->state = RX_SETUP_VALIDATE;

                    tmo_init( &fsm->dmtd_timeout, 500 );
				} else {
					fsm->state = RX_SETUP_STATE_RESET_PCS;
				}
			}
			
			break;
		}

		case RX_SETUP_VALIDATE:
		{
        	int phase, enabled;
            int rv = spll_read_ptracker(0, &phase, &enabled);
            //spll_show_stats();

			if ( rv )
			{
    			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);
       			int rx_comma_pos = (dbg0 >> 7) & 0x7f;
				ep_pcs_write( MDIO_DBG1, MDIO_DBG1_RX_ENABLE | MDIO_DBG1_TX_ENABLE | MDIO_DBG1_DMTD_SOURCE_RXRECCLK | MDIO_DBG1_COMMA_TARGET_POS(DEFAULT_COMMA_POS) );
				ep_pcs_write( MDIO_REG_MCR, MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK | MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART  );
				pp_printf("[rx-cal] RX calibration complete at phase %d ps (after %d attempts) comma @ %d taps.\n", phase, fsm->attempts, rx_comma_pos );
				spll_enable_ptracker( 0, 0 );
				usleep(100000);
       			fsm->state = RX_SETUP_DONE;

			}/* else if (tmo_expired(&fsm->dmtd_timeout))
            {
                fsm->state = RX_SETUP_STATE_RESET_PCS;
                pp_printf("Tm hit\n");
                return 0;
            }*/




			break;
		}



		case RX_SETUP_DONE:
		{
			uint16_t dbg0 = ep_pcs_read( MDIO_DBG0);
			
			if( ! (dbg0 & MDIO_DBG0_LINK_UP ) )
			{
				pp_printf("[rx-cal]: port went down, need recalibration.\n");
				fsm->state = RX_SETUP_STATE_INIT;

				return 0;
			}

			return 1;
			break;
		}
	}


	return 0;
}

timeout_t cmon_tmo;

int phy_calibration_poll()
{
    #if 0
    if( tmo_expired( &cmon_tmo) )
    {
        tmo_restart( &cmon_tmo );
        wb_cm_read( &cmon );
        wb_cm_show( &cmon );

    }
    #endif
    tx_fsm_update();
    rx_fsm_update();
    return 0;
}



void phy_calibration_init()
{
    tmo_init( &cmon_tmo, 2000 );

    pp_printf("reset phy\n");
    ep_pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN);	/* reset the PHY */
	timer_delay_ms(200);
	ep_pcs_write(MDIO_REG_MCR, MDIO_MCR_RESET);	/* reset the PHY */
	ep_pcs_write(MDIO_REG_MCR, 0);	/* reset the PHY */

    ep_pcs_write(MDIO_DBG1, 0xcafe);

    pp_printf("PLL lock: ");
 	spll_init( SPLL_MODE_FREE_RUNNING_MASTER, 0, 0 );
	while ( !spll_check_lock( 0 ))
    {
        pp_printf(".");
        usleep(100000);
    }

    spll_set_ptracker_average_samples( 10 );

    pp_printf("\n");
    tx_fsm_init(&tx_state);
    rx_fsm_init(&rx_state);

    wrc_task_create( "phy-cal", NULL, phy_calibration_poll );
}
