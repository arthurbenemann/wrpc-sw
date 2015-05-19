#include "board.h"
#include "hw/wrc_syscon_regs.h"

void timer_init()
{
	writel( SYSC_TCR_ENABLE, BASE_SYSCON + SYSC_REG_TCR );
}

uint32_t timer_get_tics()
{
	pp_printf("getTics: 0x%x\n", BASE_SYSCON + SYSC_REG_TVR, readl( BASE_SYSCON + SYSC_REG_TVR ));
	return readl( BASE_SYSCON + SYSC_REG_TVR );
}