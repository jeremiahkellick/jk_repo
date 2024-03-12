#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/metrics/metrics.h>
// #jk_build dependencies_end

#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    uint64_t milliseconds_to_wait = 1000;
    if (argc == 2) {
        milliseconds_to_wait = atol(argv[1]);
    }

    uint64_t os_freq = jk_os_timer_frequency_get();
    printf("OS frequency: %llu\n", os_freq);

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
    uint64_t cpu_freq = os_freq * cpu_elapsed / os_elapsed;

    printf("OS timer: %llu -> %llu = %llu elapsed\n", os_start, os_end, os_elapsed);
    printf("OS seconds: %.4f\n", (double)os_elapsed / (double)os_freq);

    printf("CPU timer: %llu -> %llu = %llu elapsed\n", cpu_start, cpu_end, cpu_elapsed);
    printf("CPU freq: %llu (estimated)\n", cpu_freq);

    printf("jk_cpu_timer_frequency_estimate(%llu): %llu\n",
            milliseconds_to_wait,
            jk_cpu_timer_frequency_estimate(milliseconds_to_wait));

    return 0;
}
