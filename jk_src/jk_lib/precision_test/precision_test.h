#ifndef JK_PRECISION_TEST_H
#define JK_PRECISION_TEST_H

#include <jk_src/jk_lib/jk_lib.h>

typedef struct JkPrecisionTestReference {
    double input;
    double output;
} JkPrecisionTestReference;

typedef struct JkPrecisionTestResult {
    double diff_count;
    double diff_total;
    double diff_max;
    double input_at_diff_max;
    char label[64];
} JkPrecisionTestResult;

typedef struct JkPrecisionTest {
    double input;
    uint64_t step_index;
    uint64_t result_index;
    uint64_t result_count;
    JkPrecisionTestResult results[256];
} JkPrecisionTest;

JK_PUBLIC void jk_precision_test_check_reference(char *label,
        double (*function)(double),
        uint32_t reference_count,
        JkPrecisionTestReference *references);

JK_PUBLIC b32 jk_precision_test(JkPrecisionTest *t, double min, double max, uint64_t step_count);

JK_PUBLIC void jk_precision_test_result(
        JkPrecisionTest *t, double reference, double value, char *label, ...);

JK_PUBLIC void jk_precision_test_print(JkPrecisionTest *t);

#endif
