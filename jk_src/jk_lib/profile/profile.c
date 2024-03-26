#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/utils.h>
// #jk_build dependencies_end

#include "profile.h"

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

#elif __arm64__

JK_PUBLIC uint64_t jk_cpu_timer_get(void)
{
    uint64_t timebase;
    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(timebase));
    return timebase;
}

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

static JkProfile jk_profile;

JK_PUBLIC void jk_profile_begin(void)
{
    jk_profile.start = jk_cpu_timer_get();
}

JK_PUBLIC void jk_profile_end_and_print(void)
{
    uint64_t total = jk_cpu_timer_get() - jk_profile.start;
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);
    printf("Total time: %.4fms (CPU freq %llu)\n",
            (double)total / (double)frequency,
            (long long)frequency);

    for (size_t i = 0; i < JK_ARRAY_COUNT(jk_profile.entries); i++) {
        JkProfileEntry *entry = &jk_profile.entries[i];
        if (entry->elapsed == 0) {
            continue;
        }

        uint64_t elapsed_exclude_children = entry->elapsed - entry->elapsed_children;
        printf("\t%s: %llu (%.2f%%",
                entry->name,
                (long long)elapsed_exclude_children,
                (double)elapsed_exclude_children / (double)total * 100.0);
        if (elapsed_exclude_children != entry->elapsed) {
            printf(", %.2f%% w/ children", (double)entry->elapsed / (double)total * 100.0);
        }
        printf(")\n");
    }
}

JK_PUBLIC size_t jk_profile_time_begin(char *name, size_t entry_index)
{
    JkProfileEntry *entry = &jk_profile.entries[entry_index];
    if (entry->recursive_calls++) {
        return entry_index;
    }

    entry->parent_index = jk_profile.parent_index;
    jk_profile.parent_index = entry_index;

    entry->name = name;
    entry->start = jk_cpu_timer_get();
    return entry_index;
}

JK_PUBLIC void jk_profile_time_end(size_t entry_index)
{
    JkProfileEntry *entry = jk_profile.entries + entry_index;
    if (--entry->recursive_calls != 0) {
        return;
    }

    uint64_t elapsed = jk_cpu_timer_get() - entry->start;

    entry->start = 0;
    entry->elapsed += elapsed;
    jk_profile.entries[entry->parent_index].elapsed_children += elapsed;
    jk_profile.parent_index = entry->parent_index;
}
