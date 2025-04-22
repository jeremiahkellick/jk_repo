#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
#include <jk_src/perfaware/part4/compute_polynomial.h>
// #jk_build dependencies_end

#include "listing_0187_arcsine_coefficients.c"

double inv_sqrt_2;

int main(void)
{
    inv_sqrt_2 = 1.0 / sqrt(2.0);

    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, 0.0, inv_sqrt_2, 100000000)) {
        double reference = asin(test.input);
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(ArcsineRadiansC_Taylor);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(test.input, ArcsineRadiansC_Taylor, coefficient_count),
                    "asin_taylor %d",
                    coefficient_count);
        }
        for (int coefficient_count = 2; coefficient_count < JK_ARRAY_COUNT(ArcsineRadiansC_MFTWP);
                coefficient_count++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(test.input,
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
