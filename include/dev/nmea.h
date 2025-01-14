/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __NMEA_H
#define __NMEA_H

#include <stdint.h>
#include <hw/nmea_master.h>

void nmea_init(struct nmea_master *dev, uint32_t baudrate, uint32_t invert);
int nmea_set_baud(struct nmea_master *dev, uint32_t baudrate);
void nmea_set_invert(struct nmea_master *dev, int invert);
int nmea_get_invert(struct nmea_master *dev);
void nmea_get_status(struct nmea_master *dev, int *valid, int *tip);
void nmea_get_tod(struct nmea_master *dev, int *hour, int *min, int *sec);
void nmea_get_date(struct nmea_master *dev, int *day, int *month, int *year);

#endif