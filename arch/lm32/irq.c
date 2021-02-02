/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2011 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "irq.h"

void disable_irq(void)
{
	unsigned int ie, im;
	unsigned int Mask = ~1;

	/* disable peripheral interrupts in case they were enabled */

}

void enable_irq(void)
{
	unsigned int ie, im;
	unsigned int Mask = 1;

	/* disable peripheral interrupts in-case they were enabled */
}
