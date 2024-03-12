#include <stdint.h>

#include "metrics.h"

#if _WIN32

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <windows.h>

JK_PUBLIC uint64_t jk_os_timer_get(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

JK_PUBLIC uint64_t jk_cpu_timer_get(void)
{
    return __rdtsc();
}

#else

#include <sys/time.h>

JK_PUBLIC uint64_t jk_os_timer_get(void)
{
    struct timeval value;
    gettimeofday(&value, 0);
    return jk_os_timer_frequency_get() * (uint64_t)value.tv_sec + (uint64_t)value.tv_usec;
}

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void)
{
    return 1000000;
}

#ifdef __x86_64__

#include <x86intrin.h>

JK_PUBLIC uint64_t jk_cpu_timer_get(void)
{
    return __rdtsc();
}

#elif __arm__

#endif

#endif

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait)
{
    uint64_t os_freq = jk_os_timer_frequency_get();

    uint64_t cpu_start = jk_cpu_timer_get();

    uint64_t os_start = jk_os_timer_get();
    uint64_t os_end = 0;
    uint64_t os_elapsed = 0;
    uint64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;
    while (os_elapsed < os_wait_time) {
        os_end = jk_os_timer_get();
        os_elapsed = os_end - os_start;
    }

    uint64_t cpu_end = jk_cpu_timer_get();
    uint64_t cpu_elapsed = cpu_end - cpu_start;

    return os_freq * cpu_elapsed / os_elapsed;
}
