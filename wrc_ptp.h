#ifndef __WRC_PTP_H
#define __WRC_PTP_H

#define WRC_MODE_UNKNOWN 0
#define WRC_MODE_GM 1
#define WRC_MODE_MASTER 2
#define WRC_MODE_SLAVE 3
#define WRC_MODE_PCLOCK 4

#define WRC_E2E 0
#define WRC_P2P 1

extern int ptp_mode;

int wrc_ptp_init();
int wrc_ptp_set_mode(int mode);
int wrc_ptp_get_mode();
void wrc_ptp_set_sync_mech(int mech);
int wrc_ptp_get_sync();
int wrc_ptp_start();
int wrc_ptp_stop();
int wrc_ptp_update();

#endif
