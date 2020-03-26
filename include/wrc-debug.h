/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

#ifndef __WRC_DEBUG_H
#define __WRC_DEBUG_H

#define WRC_DEFINE_TRACE_MSG(config_param, subsys_name, trace_func_name) \
    static inline void trace_func_name(const char *fmt, ...)             \
    {                                                                    \
        if (CONFIG_TRACE_ALL || config_param)                            \
        {                                                                \
            va_list vargs;                                               \
            va_start(vargs, fmt);                                        \
            pp_printf("[" #subsys_name "] ");                            \
            pp_vprintf(fmt, vargs);                                      \
            va_end(vargs);                                               \
        }                                                                \
    }

WRC_DEFINE_TRACE_MSG(CONFIG_TRACE_MAIN, "main", main_dbg)
WRC_DEFINE_TRACE_MSG(CONFIG_TRACE_STORAGE, "storage", storage_dbg)
WRC_DEFINE_TRACE_MSG(CONFIG_TRACE_DEVICES, "dev", dev_dbg)

#endif
