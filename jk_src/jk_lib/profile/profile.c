#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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

JK_PUBLIC JkProfileEntry *jk_profile_begin(char *name)
{
    if (jk_profile.count >= JK_PROFILE_MAX_ENTRIES) {
        fprintf(stderr,
                "jk_profile_begin: JK_PROFILE_MAX_ENTRIES (%d) exceeded\n",
                JK_PROFILE_MAX_ENTRIES);
        exit(1);
    }
    JkProfileEntry *entry = &jk_profile.entries[jk_profile.count++];
    if (jk_profile.current == NULL) {
        jk_profile.current = entry;
    } else if (jk_profile.current->first_child == NULL) {
        jk_profile.current->first_child = entry;
    } else {
        JkProfileEntry *prev_sibling = jk_profile.current->first_child;
        while (prev_sibling->next_sibling != NULL) {
            prev_sibling = prev_sibling->next_sibling;
        }
        prev_sibling->next_sibling = entry;
    }
    entry->name = name;
    entry->start = jk_cpu_timer_get();
    entry->parent = jk_profile.current;
    jk_profile.current = entry;
    return entry;
}

JK_PUBLIC void jk_profile_end(JkProfileEntry *entry)
{
    assert(!entry->end);
    entry->end = jk_cpu_timer_get();
    jk_profile.current = entry->parent;
}

static void jk_profile_print_rec(JkProfileEntry *entry, int depth, uint64_t total)
{
    for (int i = 0; i < depth; i++) {
        printf("\t");
    }
    uint64_t elapsed = entry->end - entry->start;
    printf("%s: %llu (%.2f%%)\n",
            entry->name,
            (long long)elapsed,
            (double)elapsed / (double)total * 100.0);
    JkProfileEntry *child = entry->first_child;
    while (child) {
        jk_profile_print_rec(child, depth + 1, elapsed);
        child = child->next_sibling;
    }
}

JK_PUBLIC void jk_profile_print(void)
{
    JkProfileEntry *root = &jk_profile.entries[0];
    uint64_t elapsed_total = root->end - root->start;
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);
    printf("%s: %.4fms (CPU freq %llu)\n",
            root->name,
            (double)elapsed_total / (double)frequency,
            (long long)frequency);

    JkProfileEntry *child = root->first_child;
    while (child) {
        jk_profile_print_rec(child, 1, elapsed_total);
        child = child->next_sibling;
    }
}
