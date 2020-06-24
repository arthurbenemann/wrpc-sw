#include <hw/gen10mhz-regs.h>
#include "board.h"

#include "gen10mhz.h"

volatile struct GEN10_WB *GEN10;

void gen10mhz_init(void)
{
    GEN10 = (volatile struct PPSG_WB *)BASE_GEN10;

    GEN10->IOR = GEN10_IOR_TAP_SET_W(2); 

}

int gen10mhz_read(void)
{
    return GEN10_IOR_TAP_CUR_R(GEN10->IOR);
}