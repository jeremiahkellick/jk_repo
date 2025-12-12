#include <math.h>
#include <stdarg.h>
#include <stdio.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "precision_test.h"

JK_PUBLIC void jk_precision_test_check_reference(char *label,
        double (*function)(double),
        int64_t reference_count,
        JkPrecisionTestReference *references)
{
    printf("%s:\n", label);
    for (int64_t i = 0; i < reference_count; i++) {
        printf("f(%+.24f) = %+.24f [reference]\n", references[i].input, references[i].output);
        double output = function(references[i].input);
        printf("                               = %+.24f (%+.24f) [%s]\n",
                output,
                references[i].output - output,
                label);
    }
    printf("\n");
}

static double jk_precision_test_result_diff_avg(JkPrecisionTestResult result)
{
    return result.diff_total / (double)result.diff_count;
}

JK_PUBLIC b32 jk_precision_test(JkPrecisionTest *t, double min, double max, int64_t step_count)
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
        JkPrecisionTest *t, double reference, double value, char *label, ...)
{
    if (t->result_index < JK_ARRAY_COUNT(t->results)) {
        JkPrecisionTestResult *result = t->results + t->result_index++;

        if (result->label[0] == '\0') {
            va_list args;
            va_start(args, label);
            vsnprintf(result->label, JK_ARRAY_COUNT(result->label), label, args);
            va_end(args);
        }

        result->diff_count++;
        if (isnan(value)) {
            result->diff_total = INFINITY;
            result->diff_max = INFINITY;
            result->input_at_diff_max = t->input;
        } else {
            double diff = fabs(reference - value);
            result->diff_total += diff;
            if (result->diff_max < diff) {
                result->diff_max = diff;
                result->input_at_diff_max = t->input;
            }
        }
    } else {
        fprintf(stderr, "jk_precision_test_result: Ran out of result slots\n");
    }
}

static int jk_precision_test_result_compare(void *data, void *a, void *b)
{
    double a_diff_max = ((JkPrecisionTestResult *)a)->diff_max;
    double b_diff_max = ((JkPrecisionTestResult *)b)->diff_max;
    return a_diff_max < b_diff_max ? -1 : b_diff_max < a_diff_max ? 1 : 0;
}

static void jk_precision_test_result_quicksort(int64_t count, JkPrecisionTestResult *data)
{
    JkPrecisionTestResult tmp;
    jk_quicksort(data,
            count,
            JK_SIZEOF(JkPrecisionTestResult),
            &tmp,
            0,
            jk_precision_test_result_compare);
}

JK_PUBLIC void jk_precision_test_print(JkPrecisionTest *t)
{
    jk_precision_test_result_quicksort(t->result_count, t->results);

    for (int64_t i = 0; i < t->result_count; i++) {
        JkPrecisionTestResult result = t->results[i];
        printf("%+.24f (%+.24f) [%s]\n",
                result.diff_max,
                jk_precision_test_result_diff_avg(result),
                result.label);
    }
}
