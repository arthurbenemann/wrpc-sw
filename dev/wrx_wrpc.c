#include <w1.h>

#define WRX_BASE      0x00000700	//!< Base address of WRX 256 bytes mailbox DPRAM (wr-cores: Aux-Master)
#define WRX_OFFSET		0x00020000	//!< wr-cores: WB_SECONDARY_CON

#define WRX_MAGIC		0xDACE			//!< Magic word
#define WRX_VERSION		0x1				//!< Current version

#define WRX_VAL_MAC_ADDRESS		0x1		//!< MAC address valid flag

/**
 * Info structure from WRPC towards 2nd LM32
 */
typedef struct
{
	volatile uint32_t 	valid;			//!< Flags which data is valid.
    volatile uint8_t 	macAddr[6];		//!< WhiteRabbit MAC address
    volatile uint16_t   brdTemp;
    volatile uint16_t   brdTempFrac;
} WrxInfo;


/**
 * Complete structure including error checking.
 */
typedef struct {
	volatile uint16_t	magic;			//! Magic word.
	volatile uint16_t	ver;			//! Wrx client version
	volatile WrxInfo	info;			//! WrxInfo structure
} Wrx;

/* Map structure at memory location */
static volatile Wrx * _wrx = (volatile Wrx *)(WRX_BASE + WRX_OFFSET);


// Initialize WhiteRabbit Exchange 
// call at initiaization time.
void wrxInit(uint8_t mac_addr[])
{
	_wrx->magic = WRX_MAGIC;
	_wrx->ver = WRX_VERSION;
	_wrx->info.valid = 0;
  pp_printf("DACE=%04X?\n", _wrx->magic);

    // mocht je zeker weten dat het mac - adreess
    if ( ( _wrx->info.valid & WRX_VAL_MAC_ADDRESS ) == 0)
    {
        // set MAC address, if possible
        memcpy(_wrx->info.macAddr, mac_addr, sizeof(_wrx->info.macAddr));

        _wrx->info.valid |= WRX_VAL_MAC_ADDRESS;	//!< Mark as set.
    }
}

// Execute on a regular basis in main-loop
void wrxExecute()
{
	// retrieve info structure for easy access
	WrxInfo * info = &(_wrx->info);
	
	int32_t  temp;

    // like in monitor/monitor.c and monitor/monitor_ppsi.c
    temp = w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_COLLECT);
    w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_NOWAIT);

    info->brdTemp     = temp >> 16;
    info->brdTempFrac = (int)((temp & 0xffff) * 10 * 1000 >> 16);

}

