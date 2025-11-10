#include <stdio.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

// This was originally an asin core function, but I had to delete some FMA's to nudge the compiler
// into producting output that better exposes issues in some of the optmization prevention
// techniques used here. So this won't actually compute asin, but all we needed was some arbitrary
// work anyway.
JK_PUBLIC double arbitrary_work(double x_squared)
{
    __m128d x_squared_sd = _mm_set_sd(x_squared);
    __m128d result = _mm_set_sd(0x1.8978c6502660ap-2);
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.0a98c5604a5c6p0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.5d065bf34c03ep0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.0e0b5512f8d35p0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.1f42350f23ccep-1));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.850e0d65729e1p-3));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.007f36ef69d66p-4));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.34a6d9f27428dp-8));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.2f8bd23b33763p-6));
    result = _mm_mul_sd(result, _mm_sqrt_sd(_mm_setzero_pd(), x_squared_sd));
    return _mm_cvtsd_f64(result);
}

static void recommended_way(uint32_t rep_count)
{
    for (uint32_t i = 0; i < rep_count; i++) {
        double value = 0.5;

#if __clang__
        __asm__ volatile("" ::"g"(&value) : "memory");
#endif
        double result = arbitrary_work(value);
#if __clang__
        __asm__ volatile("" ::"g"(&result) : "memory");
#endif

        (void)result;
    }
}

static void our_way(uint32_t rep_count)
{
    for (uint32_t i = 0; i < rep_count; i++) {
        double value = 0.5;

#if __clang__
        __asm__ volatile("" : "=x"(value));
#endif
        double result = arbitrary_work(value);
#if __clang__
        __asm__ volatile("" ::"x"(result));
#endif

        (void)result;
    }
}

static void fix_attempt_1(uint32_t rep_count)
{
    for (uint32_t i = 0; i < rep_count; i++) {
        double value;

#if __clang__
        __asm__ volatile("movsd %1, %0" : "=x"(value) : "x"(0.5));
#endif
        double result = arbitrary_work(value);
#if __clang__
        __asm__ volatile("" ::"x"(result));
#endif

        (void)result;
    }
}

static void fix_attempt_2(uint32_t rep_count)
{
    for (uint32_t i = 0; i < rep_count; i++) {
        double value;

#if __clang__
        __asm__ volatile("movapd %1, %0" : "=x"(value) : "x"(0.5));
#endif
        double result = arbitrary_work(value);
#if __clang__
        __asm__ volatile("" ::"x"(result));
#endif

        (void)result;
    }
}

typedef void LoopFunction(uint32_t rep_count);

typedef struct TestFunction {
    char *name;
    LoopFunction *func;
} TestFunction;

static TestFunction functions[] = {
    {"recommended_way", recommended_way},
    {"our_way", our_way},
    {"fix_attempt_1", fix_attempt_1},
    {"fix_attempt_2", fix_attempt_2},
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(functions)];

int main(void)
{
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (uint32_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        TestFunction *function = functions + i;
        JkPlatformRepetitionTest *test = tests + i;

        printf("\n%s\n", function->name);

        jk_platform_repetition_test_run_wave(test, 0, frequency, 10);
        while (jk_platform_repetition_test_running(test)) {
            jk_platform_repetition_test_time_begin(test);
            function->func(10000000);
            jk_platform_repetition_test_time_end(test);
        }
    }

    return 0;
}
