#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
#include <jk_src/perfaware/part4/compute_polynomial.h>
// #jk_build dependencies_end

#include "listing_0184_sine_coefficients.c"

static double compute_sin_from_table(double x, double *coefficients, uint64_t coefficient_count)
{
    double sign = x < 0.0 ? -1.0 : 1.0;
    x = fabs(x);
    x = JK_PI / 2.0 - fabs(JK_PI / 2.0 - x);

    double result = compute_polynomial(x, coefficients, coefficient_count);

    return sign * result;
}

int main(void)
{
    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        double reference = sin(test.input);
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(SineRadiansC_Taylor);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_sin_from_table(test.input, SineRadiansC_Taylor, coefficient_count),
                    "sin_taylor %d",
                    coefficient_count);
        }
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(SineRadiansC_MFTWP);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_sin_from_table(
                            test.input, SineRadiansC_MFTWP[coefficient_count], coefficient_count),
                    "sin_mftwp %d",
                    coefficient_count);
        }
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
