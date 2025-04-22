#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
#include <jk_src/perfaware/part4/compute_polynomial.h>
// #jk_build dependencies_end

#include "listing_0184_sine_coefficients.c"

double sin_mftwp_unrolled(double x)
{
    double x_squared = x * x;

    double result = fma(0x1.883c1c5deffbep-49, x_squared, -0x1.ae43dc9bf8ba7p-41);
    result = fma(result, x_squared, 0x1.6123ce513b09fp-33);
    result = fma(result, x_squared, -0x1.ae6454d960ac4p-26);
    result = fma(result, x_squared, 0x1.71de3a52aab96p-19);
    result = fma(result, x_squared, -0x1.a01a01a014eb6p-13);
    result = fma(result, x_squared, 0x1.11111111110c9p-7);
    result = fma(result, x_squared, -0x1.5555555555555p-3);
    result = fma(result, x_squared, 0x1p0);

    return result * x;
}

int main(void)
{
    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        double reference = sin(test.input);
        jk_precision_test_result(
                &test, reference, sin_mftwp_unrolled(test.input), "sin_mftwp_unrolled");
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(SineRadiansC_Taylor);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(test.input, SineRadiansC_Taylor, coefficient_count),
                    "sin_taylor %d",
                    coefficient_count);
        }
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(SineRadiansC_MFTWP);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(
                            test.input, SineRadiansC_MFTWP[coefficient_count], coefficient_count),
                    "sin_mftwp %d",
                    coefficient_count);
        }
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
