#include "onewire.h"
#include "wrx_proto.h"
#include "endpoint.h"
#include "temperature.h"
#include "sfp.h"
#include <string.h>
#include <ppsi/ppsi.h>
#include <pps_gen.h>
#include "wrx_wrpc.h"


#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

#define WRX_BASE      0x00000700	//!< Base address of WRX 256 bytes mailbox DPRAM (wr-cores: Aux-Master)
#define WRX_OFFSET		0x00020000	//!< wr-cores: WB_SECONDARY_CON

extern struct pp_instance ppi_static;


/* Map structure at memory location */
static volatile Wrx * _wrx = (volatile Wrx *)(WRX_BASE + WRX_OFFSET);


// Initialize WhiteRabbit Exchange 
// call at initiaization time.
void wrxInit(uint8_t mac_addr[])
{
	  _wrx->magic = WRX_MAGIC;
	  _wrx->ver = WRX_VERSION;
    _wrx->info.status = 0;

    // 
    if ( ( _wrx->info.status & WRX_STATUS_MAC_ADDRESS_VALID ) == 0)
    {
        // set MAC address, if possible
        memcpy((void*)_wrx->info.macAddr, mac_addr, sizeof(_wrx->info.macAddr));

        _wrx->info.status |= WRX_STATUS_MAC_ADDRESS_VALID;	//!< Mark as set.

        _wrx->info.pState = WRX_PTP_Uninitialized;
        _wrx->info.wState = WRX_WRS_Uninitialized;
        _wrx->info.sState = WRX_WR_Uninitialized;

    }
}


// Execute on a regular basis (once per second) in main-loop
//void wrxUpdate(int linkStatus, PtpPortDS *ptpPortDS)
void wrxUpdate(int linkStatus)
{
	  // retrieve info structure for easy access
	  volatile WrxInfo * info = &(_wrx->info);
	
    uint16_t rx_input_power = 0;
    uint16_t tx_output_power = 0;
    uint8_t sfp_temp = 0, sfp_temp_frac = 0;
    
    int32_t temp = wrc_temp_get("pcb");
    info->brdTemp     = temp >> 16;
    info->brdTempFrac = temp & 0xffff;

    sfp_read_temp(&sfp_temp, &sfp_temp_frac);
    info->sfpTemp     = sfp_temp;
    info->sfpTempFrac = sfp_temp_frac;

    sfp_a2_read_u16(SFP_ADC_RX_POWER, &rx_input_power);
    info->rxInputPower = rx_input_power;

    sfp_a2_read_u16(SFP_ADC_TX_POWER, &tx_output_power);
    info->txOutputPower = tx_output_power;

    // LINK_WENT_UP and LINK_UP are odd, LINK_WENT_DOWN and LINK_DOWN are even
    //if(linkStatus & 1) {
    if((linkStatus == 1) || (linkStatus == 3)) {
        info->status |= WRX_STATUS_LINK_UP;
    }
    else {
        info->status &= ~WRX_STATUS_LINK_UP;
    }

    struct pp_instance * ppi = &ppi_static;

    info->pState = ppi->state;

    struct wr_dsport * wrp =  WR_DSPOR(ppi);

    info->wState =wrp->wrPortState;

    struct wr_servo_state *s= &((struct wr_data *)ppi->ext_data)->servo_state;

    info->sState = s->state;

    // Service state
    info->sDeltaTX = s->delta_tx_s;
    info->sDeltaRX = s->delta_rx_s;
    info->mDeltaTX = s->delta_tx_m;
    info->mDeltaRX = s->delta_rx_m;
    info->mu       = s->picos_mu;

    info->bitslide = ep_get_bitslide();

    shw_pps_gen_get_time((uint64_t *)&(info->utcTime), NULL); 
}

// Returns: 1 if there was  a command
//          0 if there was no command
int wrxExecute(void) {
    // retrieve cmd structure
    volatile WrxCmd *  cmd  = &(_wrx->cmd);
    volatile WrxInfo * info = &(_wrx->info);
    uint16_t tmp;

    switch (cmd->code)
    {
    case WRX_COMMAND_NONE:
    default:
        return 0;
    case WRX_COMMAND_GET_SFP_VENDOR_SN:
        // get Serial number
        memcpy((void *)info->cmdreply.sfpVendorSN, sfp_pn, 16);
        info->cmdcode = WRX_COMMAND_GET_SFP_VENDOR_SN;
        break;
/*
    case WRX_COMMAND_AUTONEG_OFF:
        pcs_write(MDIO_REG_MCR, 0x0160);
        info->cmdcode = WRX_COMMAND_AUTONEG_OFF;
        break;
    case WRX_COMMAND_AUTONEG_ON:
        pcs_write(MDIO_REG_MCR, 0x1140);
        info->cmdcode = WRX_COMMAND_AUTONEG_ON;
        break;
*/  case WRX_COMMAND_GET_TUNEINFO:
        // need 2 find transceiver type, and such...
        info->cmdreply.tuneInfo.tuneproc = sfp_get_tuning_procedure();
        sfp_a2_read_u16(SFP_ADC_LASER_TEMP, &tmp);
        info->cmdreply.tuneInfo.laserTmpWl = tmp;
        info->cmdreply.tuneInfo.tuneword = sfp_get_tune_word();
        info->cmdcode = WRX_COMMAND_GET_TUNEINFO;
        
        sfp_a2_read_u16(112, &tmp);
        break;
    case WRX_COMMAND_SET_TUNEWORD:
        sfp_set_tune_word(cmd->params.tuneWord);
        break;
    case WRX_COMMAND_SET_THRESHOLD:
        //printf("Request to set threhsold index %d to %d\n",
            //cmd->params.threshold.index, cmd->params.threshold.value);
        sfp_a2_write_u16(cmd->params.threshold.index, cmd->params.threshold.value);
        break;
    }
    // reset command
    cmd->code = WRX_COMMAND_NONE;

    return 1;
}
