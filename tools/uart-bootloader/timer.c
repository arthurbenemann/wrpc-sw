#include "board.h"
#include "hw/wrc_syscon_regs.h"

void timer_init(int enable)
{
	writel( SYSC_TCR_ENABLE, BASE_SYSCON + SYSC_REG_TCR );
}

uint32_t timer_get_tics()
{
	return readl( BASE_SYSCON + SYSC_REG_TVR );
}
