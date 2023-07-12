/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright 2020-2021 CERN
 * Author: Juan David Gonzalez Cobas
 *
 * This program calls the library libertm to control an eRTM14/15 combo
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "libertm.h"
#include "private.h"
#include "display.h"
#include "spll_debug.h"

static int32_t signext32(uint32_t in, int bit)
{
	uint32_t mask = ~((1 << bit) - 1);
	if (in & (1 << bit))
		return in | mask;
	else
		return in;
}

static int prev_src = -1;

int n_samples = 3000;
int total_samples = 0;
FILE *f_out;
static int h_y, h_err;

int spll_dump_debug_data(const uint32_t *buf, size_t size)
{
	while (size--)
	{
		uint32_t x = *buf++;

		int sig = SPLL_DBG_EXTRACT_SIGNAL(x);
		int src = SPLL_DBG_EXTRACT_SOURCE(x);
		uint32_t value_raw = SPLL_DBG_EXTRACT_VALUE(x);
		int32_t value;

		switch (sig)
		{
		case SPLL_DBG_SIGNAL_ERR:
			value = signext32(value_raw, 23);
			break;
		default:
			value = value_raw;
		};

		if (prev_src != src)
		{
			//printf("%s ", dbg_source_to_string(src));
			prev_src = src;
		}

		if (sig == SPLL_DBG_SIGNAL_EVENT)
		{
			//printf(" event=%s", dbg_event_to_string(value));
		}

		if( sig == SPLL_DBG_SIGNAL_ERR && src == SPLL_DBG_SRC_HELPER )
		{
			h_err = value;
			//printf("H %d s %d\n", h_err, size);
		}
		if( sig == SPLL_DBG_SIGNAL_Y && src == SPLL_DBG_SRC_HELPER )
		{
			h_y = value;
		}
			
		
		//printf("%s=%d ", dbg_signal_to_string(sig),
		//	   value);

		if (SPLL_DBG_IS_LAST_RECORD(x))
		{
			//printf("\n");
			fprintf(f_out, "%d %d\n", h_err, h_y);
			//printf( "%d %d\n", h_err, h_y );
			total_samples++;

			if( total_samples % 200 == 0 )
			{
				fprintf(stdout,"%d/%d samples           \r", total_samples, n_samples);
				fflush(stdout);
			}

			if(total_samples == n_samples)
				return -1;
			//prev_src = -1;
		}
	}

	return 0;
}


void spll_readout_ertm14( struct ertm_status *handle, int undersample )
{

	int r = ertm_configure_spll_debug_dump(handle, 1, undersample);
	if (r)
		perror("ertm_configure_spll_debug_dump()");

	for (;;)
	{
		uint32_t buf[16384];
		size_t buf_size = 16384;
		int r = ertm_read_spll_debug_data(handle, buf, &buf_size);
		if (r >= 0)
		{
			//printf("read %llu\n", buf_size);
			if( spll_dump_debug_data(buf, buf_size) < 0)
				break;
		}
	}

	int retries=3;

	while(retries > 0)
	{
		r = ertm_configure_spll_debug_dump( handle, 0, 0 );
		if (r)
		{
			perror("ertm_configure_spll_debug_dump(), retrying");
			sleep(1);
			retries--;
		}
		else
		{
			break;
		}
	}

	fprintf(stderr, "ertm14: stopping SPLL logging...\n");
}

#if 0
void spll_readout_direct(struct board* board )
{

	// purge the SPLL debug FIFO
	int dummy;
	for(;;)
	{
		uint32_t r = board->readl(board, OFFSET_SOFTPLL + offsetof( struct SPLL_WB, DFR_HOST_CSR ) );
		if (r & SPLL_DFR_HOST_CSR_EMPTY)
			break;

		dummy = board->readl(board, OFFSET_SOFTPLL + offsetof( struct SPLL_WB, DFR_HOST_R0 ) );
		(void) dummy;
	}


	for (;;)
	{
		uint32_t buf[16384];
		size_t buf_size = 16384, cnt = 0;
		const size_t max_record_size = 256;
		int got_a_full_record = 1;

		while( cnt < buf_size - max_record_size )
		{
			uint32_t fifo_sr = board->readl(board, OFFSET_SOFTPLL + offsetof( struct SPLL_WB, DFR_HOST_CSR ) );

			if( got_a_full_record && ( fifo_sr & SPLL_DFR_HOST_CSR_EMPTY ) )
				break;

			uint32_t r = board->readl(board, OFFSET_SOFTPLL + offsetof( struct SPLL_WB, DFR_HOST_R0 ) );
			buf[cnt++] = r;
			got_a_full_record = SPLL_DBG_IS_LAST_RECORD(r) ? 1 : 0;
		}

		if( cnt > 0 )
			spll_dump_debug_data(buf, cnt);
	}
}
#endif



int main(int argc, char *argv[])
{
	static char usb[] = "/dev/ttyUSB2";
	struct ertm_status *handle = ertm_init(NULL);

	int kp_gains[128];
	int ki_gains[128];
	int n_gains = 20;

	int i;

	for(i=0;i<n_gains;i++)
	{
		kp_gains[i] = -(100 + i*500);
		ki_gains[i] = -2;
	}

	if (handle == NULL) {
		fprintf(stderr, "could not open %s\n", usb);
		exit(1);
	}

	ertm_configure_spll_debug_dump(handle, 0, 0);

	ertm_execute_shell_command( handle, "ptp stop");
	usleep(100000);
		

	while(1)
	{
		if( ertm_wr_diags(handle, &handle->state->wr_status) < 0 )
		{
			fprintf(stderr,"Failed to read wdiags. retrying.\n");
			usleep(300000);
			continue;
		}

		int link_up = handle->state->wr_status.WDIAG_PSTAT & WRC_DIAGS_WDIAG_PSTAT_LINK;

		printf("link_up: %d\n", link_up);

		if(link_up)
			break;

		sleep(1);
	}

	ertm_execute_shell_command( handle, "pll gain -1 0 -150 -2");
	usleep(100000);

for(i=0;i<n_gains;i++){
		char cmd[64],fname[64];
		printf("Try kp=%d,ki=%d\n", kp_gains[i], ki_gains[i] );
		sprintf(cmd,"pll gain -1 0 %d %d\n", kp_gains[i], ki_gains[i]);
		ertm_execute_shell_command( handle, cmd);
		usleep(100000);
		ertm_execute_shell_command( handle, "pll init 3 0 0");
		sleep(10);
		total_samples = 0;
		sprintf(fname,"spll-helper-kp-%d.dat", kp_gains[i]);
		f_out=fopen(fname,"wb");
		spll_readout_ertm14( handle, 1 );
		fclose(f_out);
		fflush(stdout);
	}

	return 0;
}

#if 0

	int timeout = 120;
	if( argc >= 2 )
		timeout = atoi(argv[1]);

	int good_samples = 0;

	for(;;)
	{
		ertm_wr_diags(handle, &handle->state->wr_status);

		uint32_t aux0_stat = handle->state->wr_status.WDIAG_AUX0_DETAIL_STAT;
		struct ertm_wr_status *st = &handle->state->wr_status;


		//printf("aux0: %08x\n", aux0_stat );

		if( aux0_stat & WRC_DIAGS_WDIAG_AUX0_DETAIL_STAT_LOCKED )
		{
			uint32_t phase = aux0_stat & 0xffffff;
			if(good_samples == 3 )
			{
				printf("[%d,%llu,%llu,%d,%d,%d]\n", phase,
					((unsigned long long) st->WDIAG_MU_MSB << 32 ) | st->WDIAG_MU_LSB,
					((unsigned long long) st->WDIAG_DMS_MSB << 32 ) | st->WDIAG_DMS_LSB,
					st->WDIAG_ASYM,
					st->WDIAG_CKO,
					st->WDIAG_SETP );
				break;
			}

			good_samples++;
			timeout--;

			if(!timeout)
			{
				printf("Timeout!\n");
				fflush(stdout);
				ertm_exit(handle);
				return 0;
			}
		}

		fflush(stdout);

		sleep(1);
	}

	ertm_exit(handle);

	return 0;
}

#endif