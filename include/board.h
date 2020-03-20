/*
 * We build for both wr-switch and wr-node (core).
 *
 * Unfortunately, our submodules include <board.h> without using
 * our own Kconfig defines. Thus, assume wr-node unless building
 * specifically for wr-switch (which doesn't refer to submodules).
 * Same appplies to ./tools/, where we can avoid a Makefile
 * patch for add "-include ../include/generated/autoconf.h"
 *
*/

#ifndef __BOARD_H
#define __BOARD_H

#include <hw/rawmem.h>

// fixme: for eRTM15 only
#define IUART_PLATFORM_BARE_METAL

#if defined(CONFIG_TARGET_GENERIC_PHY_8BIT) || defined(CONFIG_TARGET_GENERIC_PHY_16BIT)
#  include "boards/generic/board.h"
#elif defined(CONFIG_TARGET_WR_SWITCH)
#  include "boards/wr-switch/board.h"
#elif defined(CONFIG_TARGET_AFCZ)
#  include "boards/afcz/board.h"
#elif defined(CONFIG_TARGET_ERTM14)
#  include "boards/ertm14/board.h"
#endif


int wrc_board_early_init(void);
int wrc_board_init(void);
int wrc_board_create_tasks(void);

#endif
