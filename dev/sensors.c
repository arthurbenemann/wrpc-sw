/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2016 GSI (www.gsi.de)
 * Author: Alessandro rubini
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#include <wrc.h>
#include <string.h>
#include <sensors.h>
#include <shell.h>


static struct wrc_sensor *sensors = NULL;

const char* sensor_type_string( uint8_t flags )
{
	if ( flags & WRC_SENSOR_TEMP_CELSIUS )
		return "Temperature";
	else if ( flags & WRC_SENSOR_VOLTAGE_MV )
		return "Voltage";
	else if ( flags & WRC_SENSOR_CURRENT_MA )
		return "Current";
	else
		return "?";
}

const char* sensor_unit_string( uint8_t flags )
{
	if ( flags & WRC_SENSOR_TEMP_CELSIUS )
		return "degC";
	else if ( flags & WRC_SENSOR_VOLTAGE_MV )
		return "mV";
	else if ( flags & WRC_SENSOR_CURRENT_MA )
		return "mA";
	else
		return "";
}

void wrc_register_sensors( struct wrc_sensor* s)
{
	sensors = s;
}

/*
 * Library functions
 */
struct wrc_sensor* wrc_sensor_find_by_name(char *name)
{
	struct wrc_sensor *s = sensors;
	while( s->flags )
	{
		if( !strcmp( name, s->name ) )
			return s;
		s++;
	}

	return NULL;
}

struct wrc_sensor* wrc_sensor_find_by_id(uint8_t id)
{
	struct wrc_sensor *s = sensors;
	while( s->flags )
	{
		if( s->id == id )
			return s;
		s++;
	}

	return NULL;
}

struct wrc_sensor* wrc_sensor_find_by_type(uint8_t type)
{
	struct wrc_sensor *s = sensors;
	while( s->flags )
	{
		if( s->flags & type )
			return s;
		s++;
	}

	return NULL;
}


#if 0
extern int wrc_temp_format(char *buffer, int len)
{
	struct wrc_onetemp *p;
	int l = 0, i = 0;
	int32_t t;

	for (p = wrc_temp_getnext(NULL); p; p = wrc_temp_getnext(p), i++) {
		if (l + 16 > len) {
			l += sprintf(buffer + l, " ENOSPC");
			return l;
		}
		t = p->t;
		l += sprintf(buffer + l, "%s%s:", i ? " " : "", p->name);
		if (t == TEMP_INVALID) {
			l += sprintf(buffer + l, "INVALID");
			continue;
		}
		if (t < 0) {
			t = -(signed)t;
			l += sprintf(buffer + l, "-");
		}
		l += sprintf(buffer + l,"%d.%04d", t >> 16,
			     ((t & 0xffff) * 10 * 1000 >> 16));
	}
	return l;
}

/*
 * The task
 */
void wrc_temp_init(void)
{
}

int wrc_temp_refresh(void)
{
#if 0
	struct wrc_temp *ta;
	int ret = 0;

	for (ta = __temp_begin; ta < __temp_end; ta++)
		ret += ta->read(ta);
	return (ret > 0);
#endif
}

#endif

/*
 * The shell command
 */

static int cmd_sensors(const char *args[])
{
	pp_printf("Sensors readout: \n");

	struct wrc_sensor *s = sensors;
	while( s->flags )
	{
		if( s->flags & WRC_SENSOR_VALID )
		{
			pp_printf(" - %-20s %-20s : %-05d %s\n", 
				sensor_type_string( s->flags ),
				s->name,
				s->value,
				sensor_unit_string( s->flags )
			);
		}
		s++;
	}

	return 0;
}


DEFINE_WRC_COMMAND(sensors) = {
	.name = "sensors",
	.exec = cmd_sensors,
};

