#include <math.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/precision_test/precision_test.h>
// #jk_build dependencies_end

#define TERMS_MAX 16

// Usage: coefficients[i] where power = 2i + 1
double coefficients[TERMS_MAX];

// Usage: results[i] where degree = 2i + 1
double results[TERMS_MAX];

void compute_coefficients()
{
    double factorial = 1.0;
    double sign = 1.0;
    int i = 0;
    while (i < TERMS_MAX) {
        coefficients[i] = sign / factorial;
        sign *= -1.0;
        i++;
        factorial = factorial * (2 * i * (2 * i + 1));
    }
}

// Taylor series sin (expansion from 0)

void compute_results_naive(double x)
{
    double result = 0.0;
    double x_squared = x * x;
    double x_raised = x;
    for (int i = 0; i < TERMS_MAX; i++) {
        result += coefficients[i] * x_raised;
        results[i] = result;
        x_raised *= x_squared;
    }
}

double taylor_sin_horners(double x, int term_index)
{
    double result = coefficients[term_index];
    double x_squared = x * x;
    while (term_index) {
        result = coefficients[--term_index] + x_squared * result;
    }
    return x * result;
}

int main(void)
{
    compute_coefficients();

    JkPrecisionTest test = {0};

    while (jk_precision_test(&test, -JK_PI, JK_PI, 100000000)) {
        double reference = sin(test.input);
        compute_results_naive(test.input);
        for (int i = 0; i < TERMS_MAX; i++) {
            jk_precision_test_result(&test, reference, results[i], "taylor_sin deg %d", 2 * i + 1);
            jk_precision_test_result(&test,
                    reference,
                    taylor_sin_horners(test.input, i),
                    "taylor_sin_horners deg %d",
                    2 * i + 1);
        }
    }

    printf("\n");
    jk_precision_test_print(&test);

    return 0;
}
