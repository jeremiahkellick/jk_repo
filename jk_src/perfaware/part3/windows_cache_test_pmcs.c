#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test_custom.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm_custom(uint64_t outer_loop_iterations, uint64_t inner_loop_iterations, void *data);

static uint64_t sizes[] = {
    16 * 1024,
    1 * 1024 * 1024 * 1024,
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(sizes)];

#define BUFFER_SIZE (sizes[JK_ARRAY_COUNT(sizes) - 1])

static char *pmc_names_printable[] = {
    "TotalIssues",
    "BranchInstructions",
    "BranchMispredictions",
    "CacheMisses",
};

static JkPlatformPmcNameArray pmc_names = {
    L"TotalIssues",
    L"BranchInstructions",
    L"BranchMispredictions",
    L"CacheMisses",
};

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(BUFFER_SIZE);

    JkPlatformPmcMapping pmc_mapping =
            jk_platform_pmc_map_names(pmc_names, JK_ARRAY_COUNT(pmc_names));
    JK_ASSERT(jk_platform_pmc_mapping_valid(pmc_mapping));

    JkPlatformPmcTracer pmc_tracer;
    jk_platform_pmc_trace_begin(&pmc_tracer, &pmc_mapping);

    for (;;) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            if (sizes[i] < 1024 * 1024) {
                printf("\n%.2f KiB\n", (double)sizes[i] / 1024.0);
            } else if (sizes[i] < 1024 * 1024 * 1024) {
                printf("\n%.2f MiB\n", (double)sizes[i] / (1024.0 * 1024.0));
            } else {
                printf("\n%.2f GiB\n", (double)sizes[i] / (1024 * 1024.0 * 1024.0));
            }

            JkPlatformPmcResult pmc_min = {0};
            pmc_min.counters[3] = ULLONG_MAX;

            uint64_t inner_loop_iterations = sizes[i] / 256;
            uint64_t outer_loop_iterations = BUFFER_SIZE / sizes[i];
            uint64_t byte_count = outer_loop_iterations * sizes[i];
            jk_platform_repetition_test_run_wave(test, byte_count, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                JkPlatformPmcZone pmc_zone = {0};
                jk_platform_pmc_zone_open(&pmc_tracer, &pmc_zone);

                jk_platform_repetition_test_time_begin(test);
                jk_platform_pmc_zone_collection_start(&pmc_tracer, &pmc_zone);
                read_asm_custom(outer_loop_iterations, inner_loop_iterations, data);
                jk_platform_pmc_zone_collection_stop(&pmc_tracer, &pmc_zone);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, byte_count);

                jk_platform_pmc_zone_close(&pmc_tracer, &pmc_zone);
                JkPlatformPmcResult pmc_result =
                        jk_platform_pmc_zone_result_wait(&pmc_tracer, &pmc_zone);
                if (pmc_result.counters[3] < pmc_min.counters[3]) {
                    pmc_min = pmc_result;
                }
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
            for (int j = 0; j < JK_ARRAY_COUNT(pmc_names); j++) {
                printf("%s: %llu\n",
                        pmc_names_printable[j],
                        (unsigned long long)pmc_min.counters[j]);
            }
        }
    }
}
