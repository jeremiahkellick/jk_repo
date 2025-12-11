#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
#include <jk_src/perfaware/part4/compute_polynomial.h>
// #jk_build dependencies_end

#include "listing_0187_arcsine_coefficients.c"

static double compute_asin_from_table(double x, double *coefficients, int64_t coefficient_count)
{
    b32 in_standard_range = x <= JK_INV_SQRT_2;
    if (!in_standard_range) {
        x = sqrt(1.0 - x * x);
    }

    double result = compute_polynomial(x, coefficients, coefficient_count);

    return in_standard_range ? result : (JK_PI / 2.0) - result;
}

int main(void)
{
    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, 0.0, 1.0, 100000000)) {
        double reference = asin(test.input);
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(ArcsineRadiansC_Taylor);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_asin_from_table(test.input, ArcsineRadiansC_Taylor, coefficient_count),
                    "asin_taylor %d",
                    coefficient_count);
        }
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(ArcsineRadiansC_MFTWP);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_asin_from_table(test.input,
                            ArcsineRadiansC_MFTWP[coefficient_count],
                            coefficient_count),
                    "asin_mftwp %d",
                    coefficient_count);
        }
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
