#ifndef __REGS_H
#define __REGS_H

#define SDB_ADDRESS 0x30000

extern unsigned char *BASE_MINIC;
extern unsigned char *BASE_EP;
extern unsigned char *BASE_SOFTPLL;
extern unsigned char *BASE_PPS_GEN;
extern unsigned char *BASE_SYSCON;
extern unsigned char *BASE_UART;
extern unsigned char *BASE_ONEWIRE;
extern unsigned char *BASE_ETHERBONE_CFG;

#define FMC1_EEPROM_ADR 0x50
#define FMC2_EEPROM_ADR 0x52
#define CARRIER_EEPROM_ADR 0x55
#define FMC_EEPROM_ADR CARRIER_EEPROM_ADR //TODO: FMC is the old name as used by SPEC.

void sdb_find_devices(void);
void sdb_print_devices(void);

#endif
