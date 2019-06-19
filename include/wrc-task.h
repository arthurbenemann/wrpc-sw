/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#ifndef __WRC_TASK_H__
#define __WRC_TASK_H__

/*
 * A task is a data structure, but currently suboptimal.
 * FIXME: init must return int, and both should get a pointer to data
 * (but doing this is heavy, and forces to change the submodule too).
 */

struct wrc_task {
	int used;
	char name[16];
	int *enable;		/* A global enable variable */
	void (*init)(void);
	int (*job)(void);
	/* And we keep statistics about cpu usage */
	unsigned long nrun;
	unsigned long seconds;
	unsigned long nanos;
	unsigned long max_run_ticks; /* in ticks */
};

#define WRC_MAX_TASKS 8

extern struct wrc_task tasks[WRC_MAX_TASKS];

/* An helper for periodic tasks, relying on a static varible */
static inline int __task_not_yet(uint32_t *lastt, unsigned period,
	uint32_t now)
{
	if (!*lastt) {
		*lastt = now;
		return 0;
	}
	if (time_before(now, *lastt + period))
		return 1; /* not yet */

	*lastt += period;
	return 0;
}


#endif /* __WRC_TASK_H__ */
