#include <math.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build compiler_arguments -fno-vectorize
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define INTERLEAVE_COUNT 8

void straightforward(int64_t outer_loop_count, int64_t inner_loop_count)
{
    for (int64_t i = 0; i < outer_loop_count; i++) {
        float value = 1;
        JK_PRETEND_WRITE(value);
        for (int64_t j = 0; j < inner_loop_count; j++) {
            value = fma(value, 2, 1);
        }
        JK_PRETEND_READ(value);
    }
}

void interleaved(int64_t outer_loop_count, int64_t inner_loop_count)
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

void compressed(int64_t outer_loop_count, int64_t inner_loop_count)
{
    for (int64_t outer_index = 0; outer_index < outer_loop_count; outer_index++) {
        double values[INTERLEAVE_COUNT];
        for (int64_t i = 0; i < INTERLEAVE_COUNT; i++) {
            values[i] = 1;
            JK_PRETEND_WRITE(values[i]);
        }
        for (int64_t inner_index = 0; inner_index < inner_loop_count; inner_index++) {
            for (int64_t i = 0; i < INTERLEAVE_COUNT; i++) {
                values[i] = fma(values[i], 2, 1);
            }
        }
        for (int64_t i = 0; i < INTERLEAVE_COUNT; i++) {
            JK_PRETEND_READ(values[i]);
        }
    }
}

typedef struct Function {
    char *name;
    void (*call)(int64_t outer_loop_count, int64_t inner_loop_count);
} Function;

static Function functions[] = {
    {.name = "straightforward", .call = straightforward},
    {.name = "interleaved", .call = interleaved},
    {.name = "compressed", .call = compressed},
};

static JkPlatformRepetitionTest tests[32][JK_ARRAY_COUNT(functions)];

#define FMA_COUNT 3600000000ll

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (int64_t test_index = 0; test_index < JK_ARRAY_COUNT(tests); test_index++) {
        int64_t chain_length = (test_index + 1) * 8;
        int64_t outer_loop_count = FMA_COUNT / (INTERLEAVE_COUNT * chain_length);

        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            JkPlatformRepetitionTest *test = &tests[test_index][func_index];
            printf("\n%lld, %s\n", chain_length, functions[func_index].name);

            jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
            while (jk_platform_repetition_test_running_baseline(test, &tests[0][func_index])) {
                jk_platform_repetition_test_time_begin(test);
                functions[func_index].call(outer_loop_count, chain_length);
                jk_platform_repetition_test_time_end(test);
            }
        }
    }

    printf("\nChain length");
    for (int64_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        printf(",%s", functions[i].name);
    }
    printf("\n");

    for (int64_t test_index = 0; test_index < JK_ARRAY_COUNT(tests); test_index++) {
        int64_t chain_length = (test_index + 1) * 8;
        printf("%lld", chain_length);
        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            printf(",%.3f", (double)FMA_COUNT / (double)tests[test_index][func_index].min.cpu_time);
        }
        printf("\n");
    }

    return 0;
}
