/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __TEMPERATURE_H__
#define __TEMPERATURE_H__

#include <stdint.h>

#define WRC_MAX_TEMPERATURES 4

#define WRC_SENSOR_TEMP_CELSIUS (1<<0)
#define WRC_SENSOR_CURRENT_MA (1<<1)
#define WRC_SENSOR_VOLTAGE_MV (1<<2)
#define WRC_SENSOR_VALID (1<<3)

#define WRC_SENSOR_INVALID_VALUE (0x80000000)

struct wrc_sensor
{
	const char* name;
	uint8_t flags;
	uint8_t id;
	int16_t value;
};

struct wrc_onetemp {
	char *name;
	int32_t t;  /* fixed point, 16.16 (signed!) */
};

struct wrc_temp {
	int used;
	int (*read)(struct wrc_temp *);
	void *data;
	struct wrc_onetemp *t; /* zero-terminated */
};

/* lib functions  */
extern uint32_t wrc_temp_get(char *name);
struct wrc_onetemp *wrc_temp_getnext(struct wrc_onetemp *);
extern int wrc_temp_format(char *buffer, int len);
void wrc_temp_init(void);
int wrc_temp_refresh(void);

#endif /* __TEMPERATURE_H__ */
