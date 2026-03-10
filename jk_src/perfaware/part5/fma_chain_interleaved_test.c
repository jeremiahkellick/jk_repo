#include <math.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build compiler_arguments -fno-vectorize
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define INTERLEAVE_COUNT 8

void fma_chain(int64_t outer_loop_count, int64_t inner_loop_count)
{
    for (int64_t i = 0; i < outer_loop_count; i++) {
        double values[INTERLEAVE_COUNT];
        for (int64_t i = 0; i < INTERLEAVE_COUNT; i++) {
            values[i] = 1;
            JK_PRETEND_WRITE(values[i]);
        }
        for (int64_t j = 0; j < inner_loop_count; j++) {
            values[0] = fma(values[0], 2, 1);
            values[1] = fma(values[1], 2, 1);
            values[2] = fma(values[2], 2, 1);
            values[3] = fma(values[3], 2, 1);
            values[4] = fma(values[4], 2, 1);
            values[5] = fma(values[5], 2, 1);
            values[6] = fma(values[6], 2, 1);
            values[7] = fma(values[7], 2, 1);
        }
        for (int64_t i = 0; i < INTERLEAVE_COUNT; i++) {
            JK_PRETEND_READ(values[i]);
        }
    }
}

static JkPlatformRepetitionTest tests[32];

#define FMA_COUNT 3600000000ll

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (int64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
        JkPlatformRepetitionTest *test = tests + i;
        int64_t chain_length = (i + 1) * 8;
        int64_t outer_loop_count = FMA_COUNT / (INTERLEAVE_COUNT * chain_length);

        printf("\n%lld\n", chain_length);

        jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
        while (jk_platform_repetition_test_running_baseline(test, tests + 0)) {
            jk_platform_repetition_test_time_begin(test);
            fma_chain(outer_loop_count, chain_length);
            jk_platform_repetition_test_time_end(test);
        }
    }

    printf("\nChain length,FMAs/cycle\n");
    for (int64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
        JkPlatformRepetitionTest *test = tests + i;
        int64_t chain_length = (i + 1) * 8;
        printf("%lld,%.3f\n", chain_length, (double)FMA_COUNT / (double)test->min.cpu_time);
    }

    return 0;
}
