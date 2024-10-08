#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#include "profile.h"

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait)
{
    uint64_t os_freq = jk_platform_os_timer_frequency();
    uint64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;

    uint64_t os_end = 0;
    uint64_t os_elapsed = 0;
    uint64_t cpu_start = jk_platform_cpu_timer_get();
    uint64_t os_start = jk_platform_os_timer_get();
    while (os_elapsed < os_wait_time) {
        os_end = jk_platform_os_timer_get();
        os_elapsed = os_end - os_start;
    }

    uint64_t cpu_end = jk_platform_cpu_timer_get();
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
    jk_profile.start = jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_profile_end_and_print(void)
{
    uint64_t total = jk_platform_cpu_timer_get() - jk_profile.start;
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);
    printf("Total time: %.4fms (CPU freq %llu)\n",
            1000.0 * (double)total / (double)frequency,
            (long long)frequency);

#if !JK_PROFILE_DISABLE
    for (size_t i = 0; i < jk_profile.entry_count; i++) {
        JkProfileEntry *entry = jk_profile.entries[i];

        assert(entry->active_count == 0
                && "jk_profile_zone_begin was called without a matching jk_profile_zone_end");

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
        printf(")");

        if (entry->byte_count) {
            double seconds = (double)entry->elapsed_inclusive / (double)frequency;
            printf(" ");
            jk_print_bytes_uint64(stdout, "%.3f", entry->byte_count);
            printf(" at ");
            jk_print_bytes_double(stdout, "%.3f", (double)entry->byte_count / seconds);
            printf("/s");
        }

        printf("\n");
    }
#endif
}

#if !JK_PROFILE_DISABLE

JK_PUBLIC void jk_profile_zone_begin(
        JkProfileTiming *timing, JkProfileEntry *entry, char *name, uint64_t byte_count)
{
    if (!entry->seen) {
        entry->seen = true;
        entry->name = name;
        entry->byte_count += byte_count;
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

    timing->start = jk_platform_cpu_timer_get();
    return;
}

JK_PUBLIC void jk_profile_zone_end(JkProfileTiming *timing)
{
    uint64_t elapsed = jk_platform_cpu_timer_get() - timing->start;

#ifndef NDEBUG
    assert(!timing->ended
            && "jk_profile_zone_end: Called multiple times for a single timing instance");
    timing->ended = true;
    timing->entry->active_count--;
    assert(timing->entry->active_count >= 0
            && "jk_profile_zone_end: Called more times than jk_profile_zone_begin for some entry");
    assert(jk_profile.current == timing->entry
            && "jk_profile_zone_end: Must end all child timings before ending their parent");
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

JK_PUBLIC void jk_repetition_test_run_wave(JkRepetitionTest *test,
        uint64_t target_byte_count,
        uint64_t frequency,
        uint64_t try_for_seconds)
{
    if (test->state == JK_REPETITION_TEST_ERROR) {
        return;
    }
    if (test->state == JK_REPETITION_TEST_UNINITIALIZED) {
        test->min.v[JK_REP_VALUE_CPU_TIMER] = UINT64_MAX;
    }
    test->state = JK_REPETITION_TEST_RUNNING;
    test->target_byte_count = target_byte_count;
    test->frequency = frequency;
    test->try_for_clocks = try_for_seconds * frequency;
    test->last_found_min_time = jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_repetition_test_time_begin(JkRepetitionTest *test)
{
    test->block_open_count++;
    test->current.v[JK_REP_VALUE_PAGE_FAULT_COUNT] -= jk_platform_page_fault_count_get();
    test->current.v[JK_REP_VALUE_CPU_TIMER] -= jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_repetition_test_time_end(JkRepetitionTest *test)
{
    test->current.v[JK_REP_VALUE_CPU_TIMER] += jk_platform_cpu_timer_get();
    test->current.v[JK_REP_VALUE_PAGE_FAULT_COUNT] += jk_platform_page_fault_count_get();
    test->block_close_count++;
}

JK_PUBLIC double jk_repetition_test_bandwidth(JkRepValues values, uint64_t frequency)
{
    double seconds = (double)values.v[JK_REP_VALUE_CPU_TIMER] / (double)frequency;
    return (double)values.v[JK_REP_VALUE_BYTE_COUNT] / seconds;
}

static void jk_repetition_test_print_values(char *name, JkRepValues values, uint64_t frequency)
{
    double test_count =
            values.v[JK_REP_VALUE_TEST_COUNT] ? (double)values.v[JK_REP_VALUE_TEST_COUNT] : 1.0;
    double v[JK_REP_VALUE_COUNT];

    for (int i = 0; i < JK_REP_VALUE_COUNT; i++) {
        v[i] = (double)values.v[i] / (double)test_count;
    }

    double seconds = v[JK_REP_VALUE_CPU_TIMER] / (double)frequency;
    printf("%s: %.0f (%.3f ms)", name, v[JK_REP_VALUE_CPU_TIMER], seconds * 1000.0);
    if (v[JK_REP_VALUE_BYTE_COUNT] > 0.0) {
        printf(" ");
        jk_print_bytes_double(stdout, "%.3f", (double)v[JK_REP_VALUE_BYTE_COUNT] / seconds);
        printf("/s");
    }
    if (v[JK_REP_VALUE_PAGE_FAULT_COUNT] > 0.0) {
        printf(" %.0f page faults (", v[JK_REP_VALUE_PAGE_FAULT_COUNT]);
        jk_print_bytes_double(
                stdout, "%.3f", v[JK_REP_VALUE_BYTE_COUNT] / v[JK_REP_VALUE_PAGE_FAULT_COUNT]);
        printf("/fault)");
    }
}

JK_PUBLIC bool jk_repetition_test_running(JkRepetitionTest *test)
{
    if (test->state != JK_REPETITION_TEST_RUNNING) {
        return false;
    }
    if (test->block_open_count != test->block_close_count) {
        jk_repetition_test_error(test,
                "JkRepetitionTest: jk_repetition_test_time_begin calls not matched one-to-one with "
                "jk_repetition_test_time_end calls\n");
        return false;
    }

    uint64_t current_time = jk_platform_cpu_timer_get();
    if (test->block_open_count > 0) {
        if (test->current.v[JK_REP_VALUE_BYTE_COUNT] != test->target_byte_count) {
            jk_repetition_test_error(test,
                    "JkRepetitionTest: Counted a different number of bytes than "
                    "target_byte_count\n");
            return false;
        }

        test->total.v[JK_REP_VALUE_TEST_COUNT]++;
        for (int i = 0; i < JK_REP_VALUE_COUNT; i++) {
            test->total.v[i] += test->current.v[i];
        }
        if (test->current.v[JK_REP_VALUE_CPU_TIMER] < test->min.v[JK_REP_VALUE_CPU_TIMER]) {
            test->min = test->current;
            test->last_found_min_time = current_time;
            printf("\r                                                                             "
                   "          \r");
            jk_repetition_test_print_values("Min", test->min, test->frequency);
        }
        if (test->current.v[JK_REP_VALUE_CPU_TIMER] > test->max.v[JK_REP_VALUE_CPU_TIMER]) {
            test->max = test->current;
        }
    }

    test->current = (JkRepValues){0};
    test->block_open_count = 0;
    test->block_close_count = 0;

    if (current_time - test->last_found_min_time > test->try_for_clocks) {
        test->state = JK_REPETITION_TEST_COMPLETE;

        // Print results
        if (test->total.v[JK_REP_VALUE_TEST_COUNT]) {
            printf("\r                                                                             "
                   "          \r");
            jk_repetition_test_print_values("Min", test->min, test->frequency);
            printf("\n");
            jk_repetition_test_print_values("Max", test->max, test->frequency);
            printf("\n");
            jk_repetition_test_print_values("Avg", test->total, test->frequency);
            printf("\n");
        }
    }

    return test->state == JK_REPETITION_TEST_RUNNING;
}

JK_PUBLIC void jk_repetition_test_count_bytes(JkRepetitionTest *test, uint64_t bytes)
{
    test->current.v[JK_REP_VALUE_BYTE_COUNT] += bytes;
}

JK_PUBLIC void jk_repetition_test_error(JkRepetitionTest *test, char *message)
{
    test->state = JK_REPETITION_TEST_ERROR;
    fprintf(stderr, "%s\n", message);
}

#endif
