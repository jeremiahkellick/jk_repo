#include <math.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
// #jk_build dependencies_end

// clang-format off
static float coeffs[][10] = {
    {0x1.921216p+0f, -0x1.fa2744p-1f, -0x1.a40c82p-4f, 0x1.2c4098p-3f, -0x1.62c3a0p-2f},
    {0x1.9222b2p+0f, -0x1.00f2bcp+0f, 0x1.962452p-5f, -0x1.9f3fdcp-2f, 0x1.f359aap-2f, -0x1.c9c136p-2f},
    {0x1.921efep+0f, -0x1.ff5cdap-1f, -0x1.81fd38p-6f, -0x1.20292ap-12f, -0x1.16e4c6p-1f, 0x1.8f664ep-1f, -0x1.1a9f50p-1f},
    {0x1.921fe2p+0f, -0x1.001a50p+0f, 0x1.4cc752p-7f, -0x1.0eb3fcp-2f, 0x1.d1551ep-2f, -0x1.2e6114p+0f, 0x1.5f68eap+0f, -0x1.7e61e6p-1f},
    {0x1.921faap+0f, -0x1.ffef0cp-1f, -0x1.14d2a8p-8f, -0x1.d112acp-4f, -0x1.53d396p-2f, 0x1.10ded6p+0f, -0x1.1b0890p+1f, 0x1.1fa76cp+1f, -0x1.056a66p+0f},
    {0x1.921fb8p+0f, -0x1.0002b2p+0f, 0x1.b8ed6ep-10f, -0x1.8c14e8p-3f, 0x1.b7ac14p-3f, -0x1.0d7c00p+0f, 0x1.5288cap+1f, -0x1.11976ep+2f, 0x1.dcacdap+1f, -0x1.71a3e8p+0f},
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

static float acos_unrolled(float x)
{
    float result = -0x1.056a66p+0f;
    result = result * x + 0x1.1fa76cp+1f;
    result = result * x + -0x1.1b0890p+1f;
    result = result * x + 0x1.10ded6p+0f;
    result = result * x + -0x1.53d396p-2f;
    result = result * x + -0x1.d112acp-4f;
    result = result * x + -0x1.14d2a8p-8f;
    result = result * x + -0x1.ffef0cp-1f;
    result = result * x + 0x1.921faap+0f;
    return result;
}

static float acos_full_range(float x)
{
    b32 positive = 0.0f <= x;
    if (!positive) {
        x = -x;
    }
    b32 in_standard_range = x <= (float)JK_INV_SQRT_2;
    if (!in_standard_range) {
        x = jk_sqrt_f32(1.0f - x * x);
    }

    float result = acos_unrolled(x);

    if (!in_standard_range) {
        result = (float)(JK_PI / 2.0) - result;
    }
    if (!positive) {
        result = (float)JK_PI - result;
    }

    return result;
}

int main(void)
{
    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, 0.0f, 1.0f / sqrtf(2.0f), 100000000)) {
        float reference = acosf(test.input);
        jk_precision_test_result(&test, reference, acos_unrolled(test.input), "acos_unrolled");
        for (int64_t i = 0; i < JK_ARRAY_COUNT(coeffs); i++) {
            jk_precision_test_result(&test,
                    reference,
                    compute_polynomial(test.input, i + 5, coeffs[i]),
                    "acos %d",
                    i + 4);
        }
    }

    while (jk_precision_test(&test, -1.0f, 1.0f, 100000000)) {
        jk_precision_test_result(
                &test, acosf(test.input), acos_full_range(test.input), "acos_full_range");
    }

    printf("\n");
    jk_precision_test_print(&test);
}
