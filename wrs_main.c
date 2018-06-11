#include <wrc.h>
#include "uart.h"
#include "softpll_ng.h"
#include "minipc.h"
#include "revision.h"
#include "system_checks.h"


int scb_ver = 33;		/* SCB version */

extern struct spll_stats stats;

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
	/* Ok, now init the devices so we can printf and delay */
	uart_init_hw();
}

int main(void)
{
	uint32_t start_tics = timer_get_tics();

	check_reset();
	stats.start_cnt++;
	_endram = ENDRAM_MAGIC;
	uart_init_hw();
	TRACE("\n");
	TRACE("WR Switch Real Time Subsystem (c) CERN 2011 - 2014\n");
	TRACE("Revision: %s, built: %s %s.\n",
	      build_revision, build_date, build_time);
	TRACE("SCB version: %d. %s\n", scb_ver,(scb_ver>=34)?"10 MHz SMC Output.":"" );
	TRACE("Start counter %d\n", stats.start_cnt);
	TRACE("--\n");

	if (stats.start_cnt > 1) {
		TRACE("!!spll does not work after restart!!\n");
		/* for sure problem is in calling second time ad9516_init,
		 * but not only */
	}
	ad9516_init(scb_ver);
	rts_init();
	rtipc_init();
	spll_very_init();

	int cmd_char;
	char cmd_array[100] = {}, cmd[100] = {};
	int i = 0, y;

	for(;;)
	{
		uint32_t tics = timer_get_tics();

		if (time_after(tics, start_tics + TICS_PER_SECOND/5)) {
			//spll_show_stats();
			start_tics = tics;
		}

		//wrs pts code
		while(1) {
			cmd_char = uart_read_byte();

			//stop if no input received
			if (cmd_char == -1)
				break;
			//if delimiter, process
			if (cmd_char == '\r') {
				TRACE("\n");
				for(y = 0; y < i; y++)
					cmd[y] = cmd_array[y];
				
				i = 0;

				TRACE("received: %s\n", cmd);

				if (strcmp(cmd, "CAFEBABE") == 0)
					TRACE("DEADBEEF\n");

				for(y = 0; y < 100; y++)
					cmd[y] = 0;
			} else {
				uart_write_byte(cmd_char);
				cmd_array[i] = cmd_char;

				if (i >= 99)
					i = 0;
				else
					i++;
			}
		}

		rts_update();
		rtipc_action();
		spll_update();
		check_stack();
	}

	return 0;
}
