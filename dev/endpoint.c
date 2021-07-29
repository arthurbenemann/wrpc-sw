/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU LGPL, version 2.1 or any later version.
 */

#include <stdio.h>
#include <string.h>
#include <wrc.h>

#include "board.h"
#include "dev/syscon.h"
#include <dev/endpoint.h>
#include "storage.h"
#include "shell.h"

#include <hw/endpoint_regs.h>
#include <hw/endpoint_mdio.h>

/* Length of a single bit on the gigabit serial link in picoseconds. Used for calculating deltaRx/deltaTx
   from the serdes bitslip value */
#define PICOS_PER_SERIAL_BIT 800



/* functions for accessing PCS (MDIO) registers */
uint16_t ep_pcs_read(struct wr_endpoint_device *dev, int location)
{
	ep_write( dev, EP_REG_MDIO_CR, EP_MDIO_CR_ADDR_W(location >> 2) );
	while (( ep_read(dev, EP_REG_MDIO_ASR) & EP_MDIO_ASR_READY) == 0) ;
	return EP_MDIO_ASR_RDATA_R(ep_read(dev, EP_REG_MDIO_ASR)) & 0xffff;
}

void ep_pcs_write(struct wr_endpoint_device *dev, int location, int value)
{
	ep_write(dev, EP_REG_MDIO_CR, EP_MDIO_CR_ADDR_W(location >> 2)
	    | EP_MDIO_CR_DATA_W(value)
	    | EP_MDIO_CR_RW );

	while (( ep_read(dev, EP_REG_MDIO_ASR) & EP_MDIO_ASR_READY) == 0) ;
}


void ep_get_mac_addr(struct wr_endpoint_device *dev, uint8_t *dev_addr)
{
	uint32_t macl = ep_read(dev, EP_REG_MACL);
	uint32_t mach = ep_read(dev, EP_REG_MACH);
	dev_addr[5] = (macl & 0x000000ff);
	dev_addr[4] = (macl & 0x0000ff00) >> 8;
	dev_addr[3] = (macl & 0x00ff0000) >> 16;
	dev_addr[2] = (macl & 0xff000000) >> 24;
	dev_addr[1] = (mach & 0x000000ff);
	dev_addr[0] = (mach & 0x0000ff00) >> 8;
}


void ep_set_mac_addr(struct wr_endpoint_device* dev, uint8_t *addr)
{
	char buf[20];
	memcpy(dev->mac_addr, addr, 6);

	mac_dbg("endpoint @ 0x%x: set MAC to %s\n", dev->base,
			format_mac(buf, dev->mac_addr));

	ep_write(dev, EP_REG_MACL, ((uint32_t) dev->mac_addr[2] << 24)
	    | ((uint32_t) dev->mac_addr[3] << 16)
	    | ((uint32_t) dev->mac_addr[4] << 8)
	    | ((uint32_t) dev->mac_addr[5]) );

	ep_write(dev, EP_REG_MACH, ((uint32_t) dev->mac_addr[0] << 8)
	    | ((uint32_t) dev->mac_addr[1]) );

	dev->flags |= EP_DEV_MAC_ADDR_SET;
}

int ep_is_mac_addr_set(struct wr_endpoint_device* dev)
{
	return (dev->flags & EP_DEV_MAC_ADDR_SET) ? 1 : 0;
}

/* Initializes the endpoint and sets its local MAC address */
void ep_init(struct wr_endpoint_device* dev, void *base_addr)
{
	dev->base = base_addr;
	dev->flags = 0;

	ep_sfp_enable(dev, 1);

#if 0
	if (!IS_WR_NODE_SIM){
		*(unsigned int *)(0x62000) = 0x2;	// reset network stuff (cleanup required!)
		*(unsigned int *)(0x62000) = 0;
	}
#endif

	ep_write(dev, EP_REG_ECR, 0);		/* disable Endpoint */
	ep_write(dev, EP_REG_VCR0, EP_VCR0_QMODE_W(3));	/* disable VLAN unit - not used by WRPC */
	ep_write(dev, EP_REG_RFCR, EP_RFCR_MRU_W(1518));	/* Set the max RX packet size */
	ep_write(dev, EP_REG_TSCR, EP_TSCR_EN_TXTS | EP_TSCR_EN_RXTS);	/* Enable timestamping */
}

void ep_reset_phy(struct wr_endpoint_device* dev)
{
	uint32_t mcr;
/* Reset the GTP Transceiver - it's important to do the GTP phase alignment every time
   we start up the software, otherwise the calibration RX/TX deltas may not be correct */
	ep_pcs_write(dev, MDIO_REG_MCR, MDIO_MCR_PDOWN);	/* reset the PHY */
	
	pp_printf("Running long PHY reset...\n");
	timer_delay_ms(10000);
	pp_printf("PHY reset complete\n");
	ep_pcs_write(dev, MDIO_REG_MCR, MDIO_MCR_RESET);	/* reset the PHY */
	ep_pcs_write(dev, MDIO_REG_MCR, 0);	/* reset the PHY */

/* Don't advertise anything - we don't want flow control */
	ep_pcs_write(dev, MDIO_REG_ADVERTISE, 0);

	mcr = MDIO_MCR_SPEED1000_MASK | MDIO_MCR_FULLDPLX_MASK;
	if (dev->flags & EP_DEV_AUTONEG_ENABLED)
		mcr |= MDIO_MCR_ANENABLE | MDIO_MCR_ANRESTART;

	ep_pcs_write(dev, MDIO_REG_MCR, mcr);
}

/* Enables/disables transmission and reception. When autoneg is set to 1,
   starts up 802.3 autonegotiation process */
int ep_enable(struct wr_endpoint_device* dev, int enabled, int autoneg)
{
	if (!enabled) {
		ep_write(dev, EP_REG_ECR, 0);
		return 0;
	}

/* Disable the endpoint */
	ep_write(dev, EP_REG_ECR, 0);

	if (!IS_WR_NODE_SIM)
		mac_dbg("MAC/Endpoint ID: %x\n", ep_read(dev, EP_REG_IDCODE) );

/* Load default packet classifier rules - see ep_pfilter.c for details */
	ep_pfilter_init_default( dev );

/* Enable TX/RX paths, reset RMON counters */
	ep_write(dev, EP_REG_ECR, EP_ECR_TX_EN | EP_ECR_RX_EN | EP_ECR_RST_CNT );

	if(autoneg)
		dev->flags |= EP_DEV_AUTONEG_ENABLED;
	else
		dev->flags &= ~EP_DEV_AUTONEG_ENABLED;

	ep_reset_phy(dev);

	return 0;
}

/* Checks the link status. If the link is up, returns non-zero
   and stores the Link Partner Ability (LPA) autonegotiation register at *lpa */
int ep_link_up(struct wr_endpoint_device* dev, uint16_t * lpa)
{
	uint16_t flags = MDIO_MSR_LSTATUS;
	volatile uint16_t msr;

	if (dev->flags & EP_DEV_AUTONEG_ENABLED)
		flags |= MDIO_MSR_ANEGCOMPLETE;

	msr = ep_pcs_read(dev, MDIO_REG_MSR);
	msr = ep_pcs_read(dev, MDIO_REG_MSR);	/* Read this flag twice to make sure the status is updated */

	if (lpa)
		*lpa = ep_pcs_read(dev, MDIO_REG_LPA);

	return (msr & flags) == flags ? 1 : 0;
}

int ep_get_bitslide(struct wr_endpoint_device* dev)
{
	return PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(ep_pcs_read(dev, MDIO_REG_WR_SPEC));
}

/* Returns the TX/RX latencies. They are valid only when the link is up. */
int ep_get_deltas(struct wr_endpoint_device* dev, uint32_t * delta_tx, uint32_t * delta_rx)
{
	/* fixme: these values should be stored in calibration block in the EEPROM on the FMC. Also, the TX/RX delays of a particular SFP
	   should be added here */
	*delta_tx = sfp_deltaTx;
	*delta_rx =
	    sfp_deltaRx +
	    PICOS_PER_SERIAL_BIT *
	    MDIO_WR_SPEC_BSLIDE_R(ep_pcs_read(dev, MDIO_REG_WR_SPEC));
	return 0;
}

int ep_cal_pattern_enable(struct wr_endpoint_device* dev)
{
	uint32_t val;
	val = ep_pcs_read(dev, MDIO_REG_WR_SPEC);
	val |= MDIO_WR_SPEC_TX_CAL;
	ep_pcs_write(dev, MDIO_REG_WR_SPEC, val);

	return 0;
}

int ep_cal_pattern_disable(struct wr_endpoint_device* dev)
{
	uint32_t val;
	val = ep_pcs_read(dev, MDIO_REG_WR_SPEC);
	val &= (~MDIO_WR_SPEC_TX_CAL);
	ep_pcs_write(dev, MDIO_REG_WR_SPEC, val);

	return 0;
}

int ep_timestamper_cal_pulse(struct wr_endpoint_device* dev)
{
	ep_write(dev, EP_REG_TSCR, ep_read(dev, EP_REG_TSCR) | EP_TSCR_RX_CAL_START );
	timer_delay_ms(1);
	return ep_read(dev, EP_REG_TSCR) & EP_TSCR_RX_CAL_RESULT ? 1 : 0;
}

int ep_sfp_enable(struct wr_endpoint_device* dev, int ena)
{
	uint32_t val;

	val = ep_pcs_read(dev, MDIO_REG_ECTRL);
	if(ena)
		val &= (~MDIO_ECTRL_SFP_TX_DISABLE);
	else
		val |= MDIO_ECTRL_SFP_TX_DISABLE;
	
	ep_pcs_write(dev, MDIO_REG_ECTRL, val);

	return 0;
}
