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

volatile int cnt = 0;
volatile int32_t temp = 0;

void wrc_scan_temp(){

	if ( cnt == 25000){
		//first read the value from previous measurement,
		//first one will be random, I know
		temp = w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_COLLECT);

		//then initiate new conversion for next loop cycle
		w1_read_temp_bus(&wrpc_w1_bus, W1_FLAG_NOWAIT);

		//mprintf("Temperature (Hexa): 0x%08x. cnt = %i \n", temp, cnt);

		cnt = 0;
	} else
		cnt++;

	//mprintf("Temperature (Hexa): 0x%08x. cnt = %i \n", temp, cnt);

	set_temperature(temp);

}
