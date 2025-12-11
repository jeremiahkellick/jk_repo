#include <math.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
// #jk_build dependencies_end

// clang-format off
static float coeffs[][10] = {
    {0x1.c3d388p-14f, 0x1.fe2af2p-1f, 0x1.389edap-6f, -0x1.9f0f3cp-3f, 0x1.d19ef0p-6f},
    {0x1.da5c3ap-18f, 0x1.ffd75ap-1f, 0x1.1f88e6p-9f, -0x1.60bec6p-3f, 0x1.8f9920p-8f, 0x1.76fa9ap-8f},
    {-0x1.aaa5e8p-22f, 0x1.0001aap+0f, -0x1.1624cep-12f, -0x1.5332bcp-3f, -0x1.017a88p-9f, 0x1.501da2p-7f, -0x1.f647bep-11f},
    {-0x1.4fa3a4p-26f, 0x1.00001ap+0f, -0x1.53641cp-16f, -0x1.5520f8p-3f, -0x1.f6b216p-13f, 0x1.1b16eap-7f, -0x1.af6260p-13f, -0x1.1ffcd6p-13f},
    {0x1.d546c0p-31f, 0x1.fffffcp-1f, 0x1.9df8f6p-20f, -0x1.555ab6p-3f, 0x1.1f5010p-15f, 0x1.0ef3b8p-7f, 0x1.2dc04ap-14f, -0x1.01b002p-12f, 0x1.20bb00p-16f},
    {0x1.2730a0p-35f, 0x1.000000p+0f, 0x1.7556f4p-24f, -0x1.5555b2p-3f, 0x1.79fe0cp-19f, 0x1.10da76p-7f, 0x1.2fe918p-17f, -0x1.b01b80p-13f, 0x1.effb5ep-19f, 0x1.012ba6p-19f},
};
// clang-format on

static float compute_polynomial(float x, int64_t coefficient_count, float *coefficients)
{
    float result = coefficients[--coefficient_count];
    while (coefficient_count) {
        result = result * x + coefficients[--coefficient_count];
    }
    return result;
}

static float sin_unrolled(float x)
{
    float result = -0x1.f647bep-11f;
    result = result * x + 0x1.501da2p-7f;
    result = result * x + -0x1.017a88p-9f;
    result = result * x + -0x1.5332bcp-3f;
    result = result * x + -0x1.1624cep-12f;
    result = result * x + 0x1.0001aap+0f;
    result = result * x + -0x1.aaa5e8p-22f;
    return result;
}

static float sin_full_range(float x)
{
    x = jk_remainder_f32(x, 2 * JK_PI);

    b32 positive = 0 <= x;
    if (!positive) {
        x = -x;
    }
    if (JK_PI / 2 < x) {
        x = JK_PI - x;
    }

    float result = sin_unrolled(x);

    if (!positive) {
        result = -result;
    }

    return result;
}

static float cos_full_range(float x)
{
    return sin_full_range(x + JK_PI / 2);
}

int main(void)
{
    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, 0, JK_PI / 2, 100000000)) {
        float reference = sinf(test.input);
        jk_precision_test_result(&test, reference, sin_unrolled(test.input), "sin_unrolled");
        for (int64_t i = 0; i < JK_ARRAY_COUNT(coeffs); i++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(test.input, i + 5, coeffs[i]),
                    "sin %d",
                    i + 4);
        }
    }

    while (jk_precision_test(&test, -10 * JK_PI, 10 * JK_PI, 100000000)) {
        jk_precision_test_result(
                &test, sinf(test.input), sin_full_range(test.input), "sin_full_range");
        jk_precision_test_result(
                &test, cosf(test.input), cos_full_range(test.input), "cos_full_range");
    }

    printf("\n");
    jk_precision_test_print(&test);
}
