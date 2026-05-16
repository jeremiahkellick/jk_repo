#include <math.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define INNER_LOOP_FMA_COUNT 8

void fma_chain(int64_t outer_loop_count, int64_t inner_loop_count) {
    for (int64_t i = 0; i < outer_loop_count; i++) {
        double value = 1;
        JK_PRETEND_WRITE(value);
        for (int64_t j = 0; j < inner_loop_count; j++) {
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
            value = fma(value, 2, 1);
        }
        JK_PRETEND_READ(value);
    }
}

static JkPlatformRepetitionTest tests[32];

#define FMA_COUNT 3600000000ll

int32_t jk_platform_entry_point(int32_t argc, char **argv) {
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (int64_t inner_loop_count = 1; inner_loop_count <= JK_ARRAY_COUNT(tests);
            inner_loop_count++) {
        JkPlatformRepetitionTest *test = tests + (inner_loop_count - 1);
        int64_t chain_length = inner_loop_count * INNER_LOOP_FMA_COUNT;
        int64_t outer_loop_count = FMA_COUNT / chain_length;

        printf("\n%lld\n", chain_length);

        jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
        while (jk_platform_repetition_test_running_baseline(test, tests + 0)) {
            jk_platform_repetition_test_time_begin(test);
            fma_chain(outer_loop_count, inner_loop_count);
            jk_platform_repetition_test_time_end(test);
        }
    }

    printf("\nChain length,FMAs/cycle\n");
    for (int64_t inner_loop_count = 1; inner_loop_count <= JK_ARRAY_COUNT(tests);
            inner_loop_count++) {
        JkPlatformRepetitionTest *test = tests + (inner_loop_count - 1);
        int64_t chain_length = inner_loop_count * INNER_LOOP_FMA_COUNT;
        printf("%lld,%.3f\n", chain_length, (double)FMA_COUNT / (double)test->min.cpu_time);
    }

    return 0;
}
