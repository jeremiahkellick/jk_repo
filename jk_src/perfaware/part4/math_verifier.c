#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "reference_tables.c"

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

static void check_hard_coded_reference(
        char *label, double (*function)(double), uint32_t reference_count, Reference *references)
{
    printf("%s:\n", label);
    for (uint32_t i = 0; i < reference_count; i++) {
        printf("f(%+.24f) = %+.24f [reference]\n", references[i].input, references[i].output);
        double output = function(references[i].input);
        printf("                               = %+.24f (%+.24f) [%s]\n",
                output,
                references[i].output - output,
                label);
    }
    printf("\n");
}

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
    return (-4.0 / (PI_64 * PI_64)) * value * value + (4.0 / PI_64) * value;
}

typedef struct JkPrecisionTestResult {
    double diff_count;
    double diff_total;
    double diff_max;
    double input_at_diff_max;
    double value_at_diff_max;
    char label[64];
} JkPrecisionTestResult;

typedef struct JkPrecisionTester {
    double input;
    uint64_t step_index;
    uint64_t result_index;
    uint64_t result_count;
    JkPrecisionTestResult results[256];
} JkPrecisionTester;

static double jk_precision_test_result_diff_avg(JkPrecisionTestResult result)
{
    return result.diff_total / (double)result.diff_count;
}

JK_PUBLIC b32 jk_precision_test(JkPrecisionTester *t, double min, double max, uint64_t step_count)
{
    t->input = min + (max - min) * (double)t->step_index / (double)(step_count - 1);
    t->result_index = t->result_count;

    b32 continue_testing = t->step_index++ < step_count;

    if (!continue_testing) { // End of test
        t->step_index = 0;
        // Print new results and bring result_count up to include them
        JkPrecisionTestResult result;
        while (t->result_count < JK_ARRAY_COUNT(t->results)
                && (result = t->results[t->result_count]).diff_count > 0) {
            printf("%+.24f (%+.24f) at %+.24f [%s]\n",
                    result.diff_max,
                    jk_precision_test_result_diff_avg(result),
                    result.input_at_diff_max,
                    result.label);
            t->result_count++;
        }
    }

    return continue_testing;
}

JK_PUBLIC void jk_precision_test_result(
        JkPrecisionTester *t, double reference, double value, char *label)
{
    if (t->result_index < JK_ARRAY_COUNT(t->results)) {
        JkPrecisionTestResult *result = t->results + t->result_index++;

        if (result->label[0] != label[0]) {
            strncpy(result->label, label, JK_ARRAY_COUNT(result->label));
        }

        double diff = fabs(reference - value);
        result->diff_count++;
        result->diff_total += diff;
        if (result->diff_max < diff) {
            result->diff_max = diff;
            result->input_at_diff_max = t->input;
        }
    } else {
        fprintf(stderr, "jk_precision_test_result: Ran out of result slots\n");
    }
}

static int jk_precision_test_result_compare(void *a, void *b)
{
    double a_diff_max = ((JkPrecisionTestResult *)a)->diff_max;
    double b_diff_max = ((JkPrecisionTestResult *)b)->diff_max;
    return a_diff_max < b_diff_max ? -1 : b_diff_max < a_diff_max ? 1 : 0;
}

static void jk_precision_test_result_quicksort(uint64_t count, JkPrecisionTestResult *data)
{
    JkPrecisionTestResult tmp;
    jk_quicksort(
            data, count, sizeof(JkPrecisionTestResult), &tmp, jk_precision_test_result_compare);
}

JK_PUBLIC void jk_precision_test_print(JkPrecisionTester *t)
{
    jk_precision_test_result_quicksort(t->result_count, t->results);

    for (uint64_t i = 0; i < t->result_count; i++) {
        JkPrecisionTestResult result = t->results[i];
        printf("%+.24f (%+.24f) [%s]\n",
                result.diff_max,
                jk_precision_test_result_diff_avg(result),
                result.label);
    }
}

int main(void)
{
    check_hard_coded_reference("sin", sin, JK_ARRAY_COUNT(ref_table_sin), ref_table_sin);
    check_hard_coded_reference("cos", cos, JK_ARRAY_COUNT(ref_table_cos), ref_table_cos);
    check_hard_coded_reference("asin", asin, JK_ARRAY_COUNT(ref_table_asin), ref_table_asin);
    check_hard_coded_reference("sqrt", sqrt, JK_ARRAY_COUNT(ref_table_sqrt), ref_table_sqrt);

    JkPrecisionTester tester = {0};

    while (jk_precision_test(&tester, -PI_64, PI_64, 100000000)) {
        double reference = sin(tester.input);
        jk_precision_test_result(&tester, reference, identity(tester.input), "fake_sin");
        jk_precision_test_result(&tester, reference, sin_deg_2(tester.input), "sin_deg_2");
    }

    while (jk_precision_test(&tester, -PI_64 / 2.0, PI_64 / 2.0, 100000000)) {
        jk_precision_test_result(&tester, cos(tester.input), identity(tester.input), "fake_cos");
    }

    while (jk_precision_test(&tester, 0.0, 1.0, 100000000)) {
        jk_precision_test_result(&tester, asin(tester.input), identity(tester.input), "fake_asin");
    }

    while (jk_precision_test(&tester, 0.0, 1.0, 100000000)) {
        double reference = sqrt(tester.input);
        jk_precision_test_result(&tester, reference, identity(tester.input), "fake_sqrt");
        jk_precision_test_result(&tester, reference, sqrtsd(tester.input), "sqrtsd");
        jk_precision_test_result(&tester, reference, sqrtss(tester.input), "sqrtss");
        jk_precision_test_result(&tester, reference, rsqrtss(tester.input), "rsqrtss");
    }

    while (jk_precision_test(&tester, 0.0, PI_64, 100000000)) {
        jk_precision_test_result(
                &tester, sin(tester.input), sin_deg_2(tester.input), "sin_deg_2 (half)");
    }

    printf("\n");
    jk_precision_test_print(&tester);

    return 0;
}
