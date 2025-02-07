#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/pmcs_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

uint64_t count_nonzeroes(uint64_t size, void *data);

#define ZONE_COUNT 32

static JkPlatformPmcZone zones[ZONE_COUNT];

#define RUN_COUNT 16
#define BUFFER_SIZE (1024llu * 1024)

static uint8_t buffers[RUN_COUNT][BUFFER_SIZE];

#define TOTAL_ISSUES_INDEX 0

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

    JkPlatformPmcMapping pmc_mapping = jk_platform_pmc_map_names(pmc_names);
    JK_ASSERT(jk_platform_pmc_mapping_valid(pmc_mapping));

    JkPlatformPmcTracer pmc_tracer;
    jk_platform_pmc_trace_begin(&pmc_tracer, &pmc_mapping);

    uint64_t nonzero_count = 0;
    for (size_t run_index = 0; run_index < RUN_COUNT; run_index++) {
        JkPlatformPmcResult pmc_min = {.time_elapsed = ULLONG_MAX};

        for (int i = 0; i < nonzero_count; i++) {
            buffers[run_index][rand() % BUFFER_SIZE] = 1;
        }

        for (int i = 0; i < 10; i++) {
            for (int zone_index = 0; zone_index < ZONE_COUNT; zone_index++) {
                jk_platform_pmc_zone_begin(&pmc_tracer, &zones[zone_index]);
                count_nonzeroes(BUFFER_SIZE, &buffers[run_index][zone_index]);
                jk_platform_pmc_zone_end(&pmc_tracer, &zones[zone_index]);
            }

            for (int zone_index = 0; zone_index < ZONE_COUNT; zone_index++) {
                JkPlatformPmcResult pmc_result =
                        jk_platform_pmc_zone_result_wait(&zones[zone_index]);
                if (pmc_result.time_elapsed < pmc_min.time_elapsed) {
                    pmc_min = pmc_result;
                }
            }
        }
        printf("\nNonzeroCount: %llu\nTimeElapsed: %llu\n",
                (unsigned long long)nonzero_count,
                (unsigned long long)pmc_min.time_elapsed);
        for (int i = 0; i < pmc_tracer.count; i++) {
            printf("%s: %llu\n", pmc_names_printable[i], (unsigned long long)pmc_min.counters[i]);
        }

        nonzero_count += 4096;
    }

    return 0;
}
