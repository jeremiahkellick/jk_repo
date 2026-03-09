#include <stdint.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part5/windows_fma_chain_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void fma_chain(int64_t total, int64_t chain_length);

static JkPlatformRepetitionTest tests[42];

#define FMA_COUNT 3600000000

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (int64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
        JkPlatformRepetitionTest *test = &tests[i];
        int64_t chain_length = (i + 1) * 6;

        printf("\n%lld\n", chain_length);

        jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
        while (jk_platform_repetition_test_running_baseline(test, tests + 0)) {
            jk_platform_repetition_test_time_begin(test);
            fma_chain(FMA_COUNT, chain_length);
            jk_platform_repetition_test_time_end(test);
        }
    }

    printf("\nChain length,FMAs/cycle\n");
    for (int64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
        int64_t chain_length = (i + 1) * 6;
        printf("%lld,%.3f\n", chain_length, (double)FMA_COUNT / (double)tests[i].min.cpu_time);
    }

    return 0;
}
