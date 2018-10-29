/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <stdio.h>
#include <wrc.h>

#include "board.h"
#include "syscon.h"
#include <endpoint.h>
#include "storage.h"

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

/* Length of a single bit on the gigabit serial link in picoseconds. Used for calculating deltaRx/deltaTx
   from the serdes bitslip value */
#define PICOS_PER_SERIAL_BIT 800

/* Number of raw phase samples averaged by the DMTD detector in the Endpoint during single phase measurement.
   The bigger, the better precision, but slower rate */
#define DMTD_AVG_SAMPLES 256

static int autoneg_enabled[wr_num_ports];
volatile struct EP_WB *EP[wr_num_ports];

/* functions for accessing PCS (MDIO) registers */
static uint16_t pcs_read(int location, int port)
{
	EP[port]->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2);
	while ((EP[port]->MDIO_ASR & EP_MDIO_ASR_READY) == 0) ;
	return EP_MDIO_ASR_RDATA_R(EP[port]->MDIO_ASR) & 0xffff;
}

static void pcs_write(int location, int value, int port)
{
	EP[port]->MDIO_CR = EP_MDIO_CR_ADDR_W(location >> 2)
	    | EP_MDIO_CR_DATA_W(value)
	    | EP_MDIO_CR_RW;

	while ((EP[port]->MDIO_ASR & EP_MDIO_ASR_READY) == 0) ;
}

/* MAC address setting */
void set_mac_addr(uint8_t dev_addr[], int port)
{
	EP[port]->MACL = ((uint32_t) dev_addr[2] << 24)
	    | ((uint32_t) dev_addr[3] << 16)
	    | ((uint32_t) dev_addr[4] << 8)
	    | ((uint32_t) dev_addr[5]);

	EP[port]->MACH = ((uint32_t) dev_addr[0] << 8)
	    | ((uint32_t) dev_addr[1]);
}

void get_mac_addr(uint8_t dev_addr[], int port)
{
	dev_addr[5] = (EP[port]->MACL & 0x000000ff);
	dev_addr[4] = (EP[port]->MACL & 0x0000ff00) >> 8;
	dev_addr[3] = (EP[port]->MACL & 0x00ff0000) >> 16;
	dev_addr[2] = (EP[port]->MACL & 0xff000000) >> 24;
	dev_addr[1] = (EP[port]->MACH & 0x000000ff);
	dev_addr[0] = (EP[port]->MACH & 0x0000ff00) >> 8;
}

/* Initializes the endpoint and sets its local MAC address */
void ep_init(uint8_t mac_addr[], int port)
{
	EP[port] = (volatile struct EP_WB *)BASE_EP[port];
	set_mac_addr(mac_addr,port);
	ep_sfp_enable(1,port);

	if (!IS_WR_NODE_SIM){
		*(unsigned int *)(0x62000) = 0x2;	// reset network stuff (cleanup required!)
		*(unsigned int *)(0x62000) = 0;
	}

	EP[port]->ECR = 0;		/* disable Endpoint */
	EP[port]->VCR0 = EP_VCR0_QMODE_W(3);	/* disable VLAN unit - not used by WRPC */
	EP[port]->RFCR = EP_RFCR_MRU_W(1518);	/* Set the max RX packet size */
	EP[port]->TSCR = EP_TSCR_EN_TXTS | EP_TSCR_EN_RXTS;	/* Enable timestamping */

/* Configure DMTD phase tracking */
	EP[port]->DMCR = EP_DMCR_EN | EP_DMCR_N_AVG_W(DMTD_AVG_SAMPLES);
}

/* Enables/disables transmission and reception. When autoneg is set to 1,
   starts up 802.3 autonegotiation process */
int ep_enable(int enabled, int autoneg, int port)
{
	uint16_t mcr;

	if (!enabled) {
		EP[port]->ECR = 0;
		return 0;
	}

/* Disable the endpoint */
	EP[port]->ECR = 0;

	if (!IS_WR_NODE_SIM)
		pp_printf("ID: %x\n", EP[port]->IDCODE);

/* Load default packet classifier rules - see ep_pfilter.c for details */
	pfilter_init_default(port);

/* Enable TX/RX paths, reset RMON counters */
	EP[port]->ECR = EP_ECR_TX_EN | EP_ECR_RX_EN | EP_ECR_RST_CNT;

	autoneg_enabled[port] = autoneg;

/* Reset the GTP Transceiver - it's important to do the GTP phase alignment every time
   we start up the software, otherwise the calibration RX/TX deltas may not be correct */
	pcs_write(MDIO_REG_MCR, MDIO_MCR_PDOWN,port);	/* reset the PHY */
	if (!IS_WR_NODE_SIM)
		timer_delay_ms(200);
	pcs_write(MDIO_REG_MCR, MDIO_MCR_RESET,port);	/* reset the PHY */
	pcs_write(MDIO_REG_MCR, 0,port);	/* reset the PHY */

/* Don't advertise anything - we don't want flow control */
	pcs_write(MDIO_REG_ADVERTISE, 0,port);

	mcr = MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK;
	if (autoneg)
		mcr |= MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART;

	pcs_write(MDIO_REG_MCR, mcr,port);
	return 0;
}

/* Checks the link status. If the link is up, returns non-zero
   and stores the Link Partner Ability (LPA) autonegotiation register at *lpa */
int ep_link_up(uint16_t * lpa,int port)
{
	uint16_t flags = MDIO_MSR_LSTATUS;
	volatile uint16_t msr;

	if (autoneg_enabled[port])
		flags |= MDIO_MSR_ANEGCOMPLETE;

	msr = pcs_read(MDIO_REG_MSR,port);
	msr = pcs_read(MDIO_REG_MSR,port);	/* Read this flag twice to make sure the status is updated */

	if (lpa)
		*lpa = pcs_read(MDIO_REG_LPA,port);

	return (msr & flags) == flags ? 1 : 0;
}

int ep_get_bitslide(int port)
{
	return PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(pcs_read(MDIO_REG_WR_SPEC,port));
}

/* Returns the TX/RX latencies. They are valid only when the link is up. */
int ep_get_deltas(uint32_t * delta_tx, uint32_t * delta_rx, int port)
{
	/* fixme: these values should be stored in calibration block in the EEPROM on the FMC. Also, the TX/RX delays of a particular SFP
	   should be added here */
	*delta_tx = sfp_deltaTx[port];
	*delta_rx =
	    sfp_deltaRx[port] +
	    PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(pcs_read(MDIO_REG_WR_SPEC,port));
	return 0;
}

int ep_cal_pattern_enable(int port)
{
	uint32_t val;
	val = pcs_read(MDIO_REG_WR_SPEC,port);
	val |= MDIO_WR_SPEC_TX_CAL;
	pcs_write(MDIO_REG_WR_SPEC, val,port);

	return 0;
}

int ep_cal_pattern_disable(int port)
{
	uint32_t val;
	val = pcs_read(MDIO_REG_WR_SPEC,port);
	val &= (~MDIO_WR_SPEC_TX_CAL);
	pcs_write(MDIO_REG_WR_SPEC, val,port);

	return 0;
}

int ep_timestamper_cal_pulse(int port)
{
	EP[port]->TSCR |= EP_TSCR_RX_CAL_START;
	timer_delay_ms(1);
	return EP[port]->TSCR & EP_TSCR_RX_CAL_RESULT ? 1 : 0;
}

int ep_sfp_enable(int ena, int port)
{
	uint32_t val;
	val = pcs_read(MDIO_REG_ECTRL,port);
	if(ena)
		val &= (~MDIO_ECTRL_SFP_TX_DISABLE);
	else
		val |= MDIO_ECTRL_SFP_TX_DISABLE;
	pcs_write(MDIO_REG_ECTRL, val,port);

	return 0;
}
