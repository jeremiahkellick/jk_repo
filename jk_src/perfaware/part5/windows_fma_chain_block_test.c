#include <math.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build compiler_arguments -fno-vectorize
// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part5/windows_fma_chain_block_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void blocks_2(int64_t chain_count, int64_t block_count);
void blocks_4(int64_t chain_count, int64_t block_count);
void blocks_8(int64_t chain_count, int64_t block_count);
void blocks_12(int64_t chain_count, int64_t block_count);
void blocks_16(int64_t chain_count, int64_t block_count);

#define INTERLEAVE_COUNT 16
#define REF_INTERLEAVE_COUNT 8

void reference(int64_t chain_count, int64_t chain_length) {
    for (int64_t chain_index = 0; chain_index < chain_count * 2; chain_index++) {
        double values[REF_INTERLEAVE_COUNT];
        for (int64_t i = 0; i < REF_INTERLEAVE_COUNT; i++) {
            values[i] = 1;
            JK_PRETEND_WRITE(values[i]);
        }
        for (int64_t inner_index = 0; inner_index < chain_length; inner_index++) {
            for (int64_t i = 0; i < REF_INTERLEAVE_COUNT; i++) {
                values[i] = fma(values[i], 2, 1);
            }
        }
        for (int64_t i = 0; i < REF_INTERLEAVE_COUNT; i++) {
            JK_PRETEND_READ(values[i]);
        }
    }
}

typedef struct Function {
    int64_t block_size;
    void (*call)(int64_t chain_count, int64_t block_count);
} Function;

static Function functions[] = {
    {.block_size = 1, .call = reference},
    {.block_size = 2, .call = blocks_2},
    {.block_size = 4, .call = blocks_4},
    {.block_size = 8, .call = blocks_8},
    {.block_size = 12, .call = blocks_12},
    {.block_size = 16, .call = blocks_16},
};

static JkPlatformRepetitionTest tests[8][JK_ARRAY_COUNT(functions)];

#define FMA_COUNT 3600000000ll

int32_t jk_platform_entry_point(int32_t argc, char **argv) {
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (int64_t chain_index = 0; chain_index < JK_ARRAY_COUNT(tests); chain_index++) {
        int64_t chain_length = (chain_index + 1) * 48;
        int64_t chain_count = FMA_COUNT / (INTERLEAVE_COUNT * chain_length);

        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            JkPlatformRepetitionTest *test = &tests[chain_index][func_index];
            Function *func = functions + func_index;

            int64_t block_count = chain_length / func->block_size;
            printf("\nchain_length %lld, block_size %lld\n", chain_length, func->block_size);

            jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
            while (jk_platform_repetition_test_running_baseline(test, &tests[0][0])) {
                jk_platform_repetition_test_time_begin(test);
                func->call(chain_count, block_count);
                jk_platform_repetition_test_time_end(test);
            }
        }
    }

    printf("\nChain length");
    for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(tests[0]); func_index++) {
        printf(",%lld blocks", functions[func_index].block_size);
    }
    printf("\n");

    for (int64_t chain_index = 0; chain_index < JK_ARRAY_COUNT(tests); chain_index++) {
        int64_t chain_length = (chain_index + 1) * 48;
        printf("%lld", chain_length);
        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            printf(",%.3f",
                    (double)FMA_COUNT / (double)tests[chain_index][func_index].min.cpu_time);
        }
        printf("\n");
    }

    return 0;
}
