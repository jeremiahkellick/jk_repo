#ifndef JK_METRICS_H
#define JK_METRICS_H

#include <stdint.h>

JK_PUBLIC uint64_t jk_os_timer_get(void);

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#endif
