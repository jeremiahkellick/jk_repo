#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "reference_tables.c"

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

typedef struct MathFunc {
    char *name;
    double (*exec)(double);
} MathFunc;

typedef struct MathFuncArray {
    uint32_t count;
    MathFunc *data;
} MathFuncArray;

static void print_diffs(Reference reference, MathFuncArray functions)
{
    printf("f(%+.16f) = %+.16f [reference]\n", reference.input, reference.output);
    for (uint32_t i = 0; i < functions.count; i++) {
        double output = functions.data[i].exec(reference.input);
        printf("                       = %+.16f (%+.16f) [%s]\n",
                output,
                reference.output - output,
                functions.data[i].name);
    }
}

static void check_against_references(
        char *label, MathFuncArray functions, uint32_t reference_count, Reference *references)
{
    printf("%s:\n", label);
    for (uint32_t i = 0; i < reference_count; i++) {
        print_diffs(references[i], functions);
    }
    printf("\n");
}

static void samples_max_diff(double (*reference_func)(double),
        MathFuncArray functions,
        double min_input,
        double max_input,
        uint32_t step_count)
{
    double step_size = (max_input - min_input) / (double)step_count;
    for (uint32_t func_index = 0; func_index < functions.count; func_index++) {
        MathFunc function = functions.data[func_index];

        b32 diff_found = 0;
        double input_at_largest_diff = 0.0;
        double largest_diff = 0.0;
        for (uint32_t i = 0; i < step_count; i++) {
            double input = min_input + step_size * (double)i;
            double diff = fabs(reference_func(input) - function.exec(input));
            if (diff > largest_diff) {
                diff_found = 1;
                input_at_largest_diff = input;
                largest_diff = diff;
            }
        }
        if (diff_found) {
            Reference reference = {
                .input = input_at_largest_diff,
                .output = reference_func(input_at_largest_diff),
            };
            printf("Largest diff for %s:\n", function.name);
            print_diffs(reference, functions);
        } else {
            printf("No differences found for %s\n", function.name);
        }
    }
}

static double identity(double x)
{
    return x;
}

static double my_sqrt(double value)
{
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(value)));
}

static double my_sqrt_32(double value)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss((float)value)));
}

static double rsqrt(double value)
{
    return 1.0 / _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss((float)value)));
}

int main(void)
{
    MathFunc sine_funcs[] = {{"sin", sin}, {"fake_sin", identity}};
    MathFuncArray sines = {.count = JK_ARRAY_COUNT(sine_funcs), .data = sine_funcs};

    MathFunc cosine_funcs[] = {{"cos", cos}, {"fake_cos", identity}};
    MathFuncArray cosines = {.count = JK_ARRAY_COUNT(cosine_funcs), .data = cosine_funcs};

    MathFunc arcsine_funcs[] = {{"asin", asin}, {"fake_asin", identity}};
    MathFuncArray arcsines = {.count = JK_ARRAY_COUNT(arcsine_funcs), .data = arcsine_funcs};

    MathFunc square_root_funcs[] = {
        {"sqrt", sqrt},
        {"fake_sqrt", identity},
        {"my_sqrt", my_sqrt},
        {"my_sqrt_32", my_sqrt_32},
        {"rsqrt", rsqrt},
    };
    MathFuncArray square_roots = {
        .count = JK_ARRAY_COUNT(square_root_funcs), .data = square_root_funcs};

    check_against_references("Sine", sines, JK_ARRAY_COUNT(ref_table_sin), ref_table_sin);
    check_against_references("Cosine", cosines, JK_ARRAY_COUNT(ref_table_cos), ref_table_cos);
    check_against_references("Arcsine", arcsines, JK_ARRAY_COUNT(ref_table_asin), ref_table_asin);
    check_against_references(
            "Square root", square_roots, JK_ARRAY_COUNT(ref_table_sqrt), ref_table_sqrt);

    samples_max_diff(sin, sines, -PI_64, PI_64, 100000000);
    samples_max_diff(cos, cosines, -PI_64 / 2.0, PI_64 / 2.0, 100000000);
    samples_max_diff(asin, arcsines, 0.0, 1.0, 100000000);
    samples_max_diff(sqrt, square_roots, 0.0, 1.0, 100000000);

    return 0;
}
