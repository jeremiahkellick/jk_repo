#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
// #jk_build dependencies_end

#define TERMS_MAX 64

// Usage: coefficients[i] where power = 2i + 1
double coefficients[TERMS_MAX];

// Usage: results[i] where degree = 2i + 1
double results[TERMS_MAX];

void compute_coefficients()
{
    int n = 1;
    double sign = 1.0;
    int i = 0;
    while (i < TERMS_MAX) {
        coefficients[i] = sign / (double)n;
        sign *= -1.0;
        i++;
        n = n * 2 * i * (2 * i + 1);
    }
}

// Taylor series sin
void compute_results(double x)
{
    double result = 0.0;
    double x_raised = x;
    for (int i = 0; i < TERMS_MAX; i++) {
        result += coefficients[i] * x_raised;
        results[i] = result;
        x_raised *= x * x;
    }
}

int main(void)
{
    compute_coefficients();

    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        double reference = sin(test.input);
        compute_results(test.input);
        int i = 0;
        while (i < TERMS_MAX) {
            jk_precision_test_result(&test, reference, results[i], "taylor_sin deg %d", 2 * i + 1);
            i += i < 7 ? 1 : 8;
        }
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
