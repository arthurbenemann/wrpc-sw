#ifndef __WRC_PTP_H
#define __WRC_PTP_H

#define WRC_MODE_UNKNOWN 0
#define WRC_MODE_GM 1
#define WRC_MODE_MASTER 2
#define WRC_MODE_SLAVE 3
#define WRC_MODE_ABSCAL 4
#define WRC_MODE_CASCADED 5

extern int ptp_mode[wr_num_ports];

int wrc_ptp_init(void);
int wrc_ptp_set_mode(int mode, int port);
int wrc_ptp_get_mode(int port);
int wrc_ptp_start(int port);
int wrc_ptp_stop(int port);
int wrc_ptp_update(void);

#endif
