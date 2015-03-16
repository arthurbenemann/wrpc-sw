/*
 *
 * Author: eml <emilio@sevensols.com>
 *
 * Function to monitor the temperature in the CPIC board.
 */

#include <w1.h>
#include <pps_gen.h>
#include <util.h>
#include <syscon.h>

void wrc_scan_temp(){

	int32_t temp;

	//first read the value from previous measurement,
	//first one will be random, I know
	temp = w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_COLLECT);
	//then initiate new conversion for next loop cycle
	w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_NOWAIT);

	term_clear();
	pcprintf(1, 1, C_BLUE, "CPIC Temperature Monitor v 1.0");
	pcprintf(2, 1, C_GREY, "Esc = exit");

	pcprintf(4, 1, C_WHITE, "Temperature: %d.%04d C", temp >> 16, (int)((temp & 0xffff) * 10 * 1000 >> 16));
	pcprintf(5, 1, C_WHITE, "Temperature (Hexa): 0x%08x \n", temp);
	set_temperature(temp);
	pp_printf("\n--\n");
}
