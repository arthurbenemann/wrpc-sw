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

#if defined(CONFIG_TARGET_GENERIC_PHY_8BIT) || defined(CONFIG_TARGET_GENERIC_PHY_16BIT)
#  include "boards/generic/board.h"
#else
#error "Unsupported board"
#endif


int wrc_board_early_init(void);
int wrc_board_init(void);
int wrc_board_create_tasks(void);

#endif
