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

typedef struct JkProfile {
    uint64_t start;

#if !JK_PROFILE_DISABLE
    JkProfileEntry *current;
    uint64_t depth;
    size_t entry_count;
    JkProfileEntry *entries[1024];
#endif
} JkProfile;

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

#if !JK_PROFILE_DISABLE
    for (size_t i = 0; i < jk_profile.entry_count; i++) {
        JkProfileEntry *entry = jk_profile.entries[i];

        assert(entry->active_count == 0
                && "jk_profile_time_begin was called without a matching jk_profile_time_end");

        for (uint64_t j = 0; j < entry->depth; j++) {
            printf("\t");
        }
        printf("\t%s[%llu]: %llu (%.2f%%",
                entry->name,
                (long long)entry->hit_count,
                (long long)entry->elapsed_exclusive,
                (double)entry->elapsed_exclusive / (double)total * 100.0);
        if (entry->elapsed_inclusive != entry->elapsed_exclusive) {
            printf(", %.2f%% w/ children",
                    (double)entry->elapsed_inclusive / (double)total * 100.0);
        }
        printf(")\n");
    }
#endif
}

#if !JK_PROFILE_DISABLE

JK_PUBLIC void jk_profile_time_begin(JkProfileTiming *timing, JkProfileEntry *entry, char *name)
{
    if (!entry->seen) {
        entry->seen = true;
        entry->name = name;
        entry->depth = jk_profile.depth;
        jk_profile.entries[jk_profile.entry_count++] = entry;
        assert(jk_profile.entry_count <= JK_ARRAY_COUNT(jk_profile.entries));
    }

    timing->parent = jk_profile.current;
    jk_profile.current = entry;
    jk_profile.depth++;

    timing->saved_elapsed_inclusive = entry->elapsed_inclusive;

#ifndef NDEBUG
    entry->active_count++;
    timing->entry = entry;
    timing->ended = false;
#endif

    timing->start = jk_cpu_timer_get();
    return;
}

JK_PUBLIC void jk_profile_time_end(JkProfileTiming *timing)
{
    uint64_t elapsed = jk_cpu_timer_get() - timing->start;

#ifndef NDEBUG
    assert(!timing->ended
            && "jk_profile_time_end: Called multiple times for a single timing instance");
    timing->ended = true;
    timing->entry->active_count--;
    assert(timing->entry->active_count >= 0
            && "jk_profile_time_end: Called more times than jk_profile_time_begin for some entry");
    assert(jk_profile.current == timing->entry
            && "jk_profile_time_end: Must end all child timings before ending their parent");
#endif

    if (timing->parent) {
        timing->parent->elapsed_exclusive -= elapsed;
    }
    jk_profile.current->elapsed_exclusive += elapsed;
    jk_profile.current->elapsed_inclusive = timing->saved_elapsed_inclusive + elapsed;
    jk_profile.current->hit_count++;

    jk_profile.current = timing->parent;
    jk_profile.depth--;
}

#endif
