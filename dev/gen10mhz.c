#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wrc.h>
#include <wrpc.h>
#include "hw/gen10mhz-regs.h"
#include "shell.h"

volatile struct GEN10_WB *gen10;

void gen10m_help(){
    pp_printf("Usage: \n");
    pp_printf("  gen10m pr  [high_cnt] \n");
	pp_printf("  gen10m dcr [low_cnt]\n");
	pp_printf("  gen10m csr [shift_cnt]\n");
}

static int cmd_gen10m(const char *args[])
{
    if (!args[0] || (!args[1])) {
      gen10m_help();
      return -1;
    }
	if (!strcasecmp(args[0], "pr")) {
		gen10->PR  = atoi(args[1]);
	} else if (!strcasecmp(args[0], "dcr")) {
		gen10->DCR  = atoi(args[1]);
	} else if (!strcasecmp(args[0], "csr")) {
		gen10->CSR  = atoi(args[1]);
	} else{
		gen10m_help();
	}
}

DEFINE_WRC_COMMAND(cdly) = {
 .name = "gen10m",
 .exec = cmd_gen10m,
};

int gen10mhz_init()
{
	gen10 = (volatile struct GEN10_WB *)BASE_GEN10MHZ_CFG;

	pp_printf("init GEN_10MHz module!!!!\n");
	gen10->PR  = 25;
	gen10->DCR = 25;
	gen10->CSR = 22;
	return 0;
}
