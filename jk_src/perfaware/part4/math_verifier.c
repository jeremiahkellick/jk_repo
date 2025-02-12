#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
// #jk_build dependencies_end

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "reference_tables.c"

static double identity(double x)
{
    return x;
}

static double sqrtsd(double value)
{
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(value)));
}

static double sqrtss(double value)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss((float)value)));
}

static double rsqrtss(double value)
{
    return _mm_cvtss_f32(_mm_rcp_ss(_mm_rsqrt_ss(_mm_set_ss((float)value))));
}

static double sin_deg_2(double value)
{
    double sign = value < 0 ? -1.0 : 1.0;
    double x = sign * value;
    return sign * ((-4.0 / (JK_PI * JK_PI)) * x * x + (4.0 / JK_PI) * x);
}

#define SQRT_OF_2 1.4142135623730950488
#define A ((8.0 - 8.0 * SQRT_OF_2) / (JK_PI * JK_PI))
#define B ((-2.0 + 4 * SQRT_OF_2) / JK_PI)

static double sin_deg_2_alt(double value)
{
    double sign = value < 0 ? -1.0 : 1.0;
    double abs = fabs(value);
    double x = JK_PI / 2.0 - fabs(-JK_PI / 2.0 + abs);
    return sign * (A * (x * x) + B * x);
}

static double cos_deg_2(double value)
{
    return sin_deg_2_alt(value + JK_PI / 2.0);
}

int main(void)
{
    jk_precision_test_check_reference("sin", sin, JK_ARRAY_COUNT(ref_table_sin), ref_table_sin);
    jk_precision_test_check_reference("cos", cos, JK_ARRAY_COUNT(ref_table_cos), ref_table_cos);
    jk_precision_test_check_reference("asin", asin, JK_ARRAY_COUNT(ref_table_asin), ref_table_asin);
    jk_precision_test_check_reference("sqrt", sqrt, JK_ARRAY_COUNT(ref_table_sqrt), ref_table_sqrt);

    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        double reference = sin(test.input);
        jk_precision_test_result(&test, reference, identity(test.input), "fake_sin");
        jk_precision_test_result(&test, reference, sin_deg_2(test.input), "sin_deg_2");
        jk_precision_test_result(&test, reference, sin_deg_2_alt(test.input), "sin_deg_2_alt");
    }

    while (jk_precision_test(&test, -JK_PI / 2.0, JK_PI / 2.0, 100000000)) {
        double reference = cos(test.input);
        jk_precision_test_result(&test, reference, identity(test.input), "fake_cos");
        jk_precision_test_result(&test, reference, cos_deg_2(test.input), "cos_deg_2");
    }

    while (jk_precision_test(&test, 0.0, 1.0, 100000000)) {
        jk_precision_test_result(&test, asin(test.input), identity(test.input), "fake_asin");
    }

    while (jk_precision_test(&test, 0.0, 1.0, 100000000)) {
        double reference = sqrt(test.input);
        jk_precision_test_result(&test, reference, identity(test.input), "fake_sqrt");
        jk_precision_test_result(&test, reference, sqrtsd(test.input), "sqrtsd");
        jk_precision_test_result(&test, reference, sqrtss(test.input), "sqrtss");
        jk_precision_test_result(&test, reference, rsqrtss(test.input), "rsqrtss");
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
