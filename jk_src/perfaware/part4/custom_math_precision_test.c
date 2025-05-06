#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
#include <jk_src/perfaware/part4/custom_math_functions.h>
// #jk_build dependencies_end

#include "reference_tables.c"

int main(void)
{
    jk_precision_test_check_reference("sin", sin, JK_ARRAY_COUNT(ref_table_sin), ref_table_sin);
    jk_precision_test_check_reference("cos", cos, JK_ARRAY_COUNT(ref_table_cos), ref_table_cos);
    jk_precision_test_check_reference("asin", asin, JK_ARRAY_COUNT(ref_table_asin), ref_table_asin);
    jk_precision_test_check_reference("sqrt", sqrt, JK_ARRAY_COUNT(ref_table_sqrt), ref_table_sqrt);

    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        jk_precision_test_result(&test, sin(test.input), jk_sin(test.input), "jk_sin");
    }

    while (jk_precision_test(&test, -JK_PI / 2.0, JK_PI / 2.0, 100000000)) {
        jk_precision_test_result(&test, cos(test.input), jk_cos(test.input), "jk_cos");
    }

    while (jk_precision_test(&test, 0.0, 1.0, 100000000)) {
        jk_precision_test_result(&test, asin(test.input), jk_asin(test.input), "jk_asin");
    }

    while (jk_precision_test(&test, 0.0, 1.0, 100000000)) {
        jk_precision_test_result(&test, sqrt(test.input), jk_sqrt(test.input), "jk_sqrt");
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
