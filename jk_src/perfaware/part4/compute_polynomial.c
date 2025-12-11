#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "compute_polynomial.h"

double compute_polynomial(double x, double *coefficients, int64_t coefficient_count)
{
    __m128d x_squared = _mm_set_sd(x * x);
    __m128d result = _mm_set_sd(coefficients[--coefficient_count]);
    while (coefficient_count) {
        result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(coefficients[--coefficient_count]));
    }
    return _mm_cvtsd_f64(result) * x;
}
