// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "custom_math_functions.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

JK_PUBLIC double jk_sin_core(double x)
{
    if (JK_PI / 2.0 < x) {
        x = JK_PI - x;
    }
    __m128d x_squared = _mm_set_sd(x * x);
    __m128d result = _mm_set_sd(0x1.883c1c5deffbep-49);
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(-0x1.ae43dc9bf8ba7p-41));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(0x1.6123ce513b09fp-33));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(-0x1.ae6454d960ac4p-26));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(0x1.71de3a52aab96p-19));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(-0x1.a01a01a014eb6p-13));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(0x1.11111111110c9p-7));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(-0x1.5555555555555p-3));
    result = _mm_fmadd_sd(result, x_squared, _mm_set_sd(0x1p0));
    return _mm_cvtsd_f64(result) * x;
}

JK_PUBLIC double jk_sin(double x)
{
    double sign = x < 0 ? -1.0 : 1.0;
    return sign * jk_sin_core(JK_ABS(x));
}

JK_PUBLIC double jk_cos(double x)
{
    return jk_sin(x + JK_PI / 2.0);
}

JK_PUBLIC double jk_asin_core(double x_squared)
{
    __m128d x_squared_sd = _mm_set_sd(x_squared);
    __m128d result = _mm_set_sd(0x1.8978c6502660ap-2);
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.0a98c5604a5c6p0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.5d065bf34c03ep0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.0e0b5512f8d35p0));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.1f42350f23ccep-1));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(-0x1.850e0d65729e1p-3));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.007f36ef69d66p-4));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.34a6d9f27428dp-8));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.2f8bd23b33763p-6));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.6ce213041c326p-6));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.f1defdcf41a11p-6));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.6db67483a8f77p-5));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.3333341adb0b8p-4));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.5555555487dd3p-3));
    result = _mm_fmadd_sd(result, x_squared_sd, _mm_set_sd(0x1.0000000000079p0));
    return _mm_cvtsd_f64(result);
}

JK_PUBLIC double jk_asin(double x)
{
    b32 in_standard_range = x <= JK_INV_SQRT_2;
    if (!in_standard_range) {
        x = jk_sqrt(1.0 - x * x);
    }

    double core_result = jk_asin_core(x * x) * x;
    return in_standard_range ? core_result : (JK_PI / 2.0) - core_result;
}

JK_PUBLIC double jk_sqrt(double x)
{
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(x)));
}
