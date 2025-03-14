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

#include "listing_0184_sine_coefficients.c"

double compute_polynomial(double x, double *coefficients, uint64_t coefficient_count)
{
    __m128d x_squared = _mm_set_sd(x * x);
    __m128d result = _mm_set_sd(coefficients[--coefficient_count]);
    while (coefficient_count) {
        result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(coefficients[--coefficient_count]));
    }
    return _mm_cvtsd_f64(result) * x;
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
