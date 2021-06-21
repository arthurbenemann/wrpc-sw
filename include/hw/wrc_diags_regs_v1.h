#ifndef __WRC_DIAGS_REGS_V1_H
#define __WRC_DIAGS_REGS_V1_H

#include <stdint.h>

struct wrc_diags_regs_v1 {
  /* [0x0]: REG Version register */
  uint32_t VER;
  /* [0x4]: REG Ctrl */
  uint32_t CTRL;
  /* [0x8]: REG WRPC Diag: servo status */
  uint32_t WDIAG_SSTAT;
  /* [0xc]: REG WRPC Diag: Port status */
  uint32_t WDIAG_PSTAT;
  /* [0x10]: REG WRPC Diag: PTP state */
  uint32_t WDIAG_PTPSTAT;
  /* [0x14]: REG WRPC Diag: AUX state */
  uint32_t WDIAG_ASTAT;
  /* [0x18]: REG WRPC Diag: Tx PTP Frame cnts */
  uint32_t WDIAG_TXFCNT;
  /* [0x1c]: REG WRPC Diag: Rx PTP Frame cnts */
  uint32_t WDIAG_RXFCNT;
  /* [0x20]: REG WRPC Diag:local time [msb of s] */
  uint32_t WDIAG_SEC_MSB;
  /* [0x24]: REG WRPC Diag: local time [lsb of s] */
  uint32_t WDIAG_SEC_LSB;
  /* [0x28]: REG WRPC Diag: local time [ns] */
  uint32_t WDIAG_NS;
  /* [0x2c]: REG WRPC Diag: Round trip (mu) [msb of ps] */
  uint32_t WDIAG_MU_MSB;
  /* [0x30]: REG WRPC Diag: Round trip (mu) [lsb of ps] */
  uint32_t WDIAG_MU_LSB;
  /* [0x34]: REG WRPC Diag: Master-slave delay (dms) [msb of ps] */
  uint32_t WDIAG_DMS_MSB;
  /* [0x38]: REG WRPC Diag: Master-slave delay (dms) [lsb of ps] */
  uint32_t WDIAG_DMS_LSB;
  /* [0x3c]: REG WRPC Diag: Total link asymmetry [ps] */
  uint32_t WDIAG_ASYM;
  /* [0x40]: REG WRPC Diag: Clock offset (cko) [ps] */
  uint32_t WDIAG_CKO;
  /* [0x44]: REG WRPC Diag: Phase setpoint (setp) [ps] */
  uint32_t WDIAG_SETP;
  /* [0x48]: REG WRPC Diag: Update counter (ucnt) */
  uint32_t WDIAG_UCNT;
  /* [0x4c]: REG WRPC Diag: Board temperature [C degree] */
  uint32_t WDIAG_TEMP;
};

#endif
