#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "jk_lib.h"

// ---- Buffer begin -----------------------------------------------------------

JK_PUBLIC void jk_buffer_zero(JkBuffer buffer)
{
    memset(buffer.data, 0, buffer.size);
}

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string)
{
    JkBuffer buffer = {.size = strlen(string), .data = (uint8_t *)string};
    return buffer;
}

JK_PUBLIC int jk_buffer_character_get(JkBuffer buffer, uint64_t pos)
{
    return pos < buffer.size ? buffer.data[pos] : EOF;
}

JK_PUBLIC int jk_buffer_character_next(JkBuffer buffer, uint64_t *pos)
{
    int c = jk_buffer_character_get(buffer, *pos);
    (*pos)++;
    return c;
}

JK_PUBLIC b32 jk_char_is_whitespace(uint8_t c)
{
    return c == ' ' || ('\t' <= c && c <= '\r');
}

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string)
{
    for (uint64_t i = 0; i < string.size; i++) {
        if (jk_char_is_whitespace(string.data[i])) {
            return 1;
        }
    }
    return 0;
}

// ---- Buffer end -------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

JK_PUBLIC void *jk_arena_alloc(JkArena *arena, uint64_t byte_count)
{
    uint64_t new_pos = arena->pos + byte_count;
    if (new_pos <= arena->memory.size) {
        void *result = arena->memory.data + arena->pos;
        arena->pos = new_pos;
        return result;
    } else {
        return 0;
    }
}

JK_PUBLIC void *jk_arena_alloc_zero(JkArena *arena, uint64_t byte_count)
{
    void *result = jk_arena_alloc(arena, byte_count);
    memset(result, 0, byte_count);
    return result;
}

JK_PUBLIC void *jk_arena_pointer_get(JkArena *arena)
{
    return arena->memory.data + arena->pos;
}

JK_PUBLIC void jk_arena_pointer_set(JkArena *arena, void *pointer)
{
    arena->pos = (uint8_t *)pointer - arena->memory.data;
}

// ---- Arena end --------------------------------------------------------------

// ---- UTF-8 begin ------------------------------------------------------------

JK_PUBLIC JkUtf8Codepoint jk_utf8_codepoint_encode(uint32_t codepoint32)
{
    JkUtf8Codepoint result = {0};
    if (codepoint32 < 0x80) {
        result.b[0] = (unsigned char)codepoint32;
    } else if (codepoint32 < 0x800) {
        result.b[0] = (unsigned char)(0xc0 | (codepoint32 >> 6));
        result.b[1] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else if (codepoint32 < 0x10000) {
        result.b[0] = (unsigned char)(0xe0 | (codepoint32 >> 12));
        result.b[1] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        result.b[2] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else {
        result.b[0] = (unsigned char)(0xf0 | (codepoint32 >> 18));
        result.b[1] = (unsigned char)(0x80 | ((codepoint32 >> 12) & 0x3f));
        result.b[2] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        result.b[3] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    }
    return result;
}

JK_PUBLIC int32_t jk_utf8_codepoint_decode(JkUtf8Codepoint codepoint)
{
    if ((codepoint.b[0] & 0x80) == 0x0) {
        return codepoint.b[0];
    } else if ((codepoint.b[0] & 0xe0) == 0xc0) {
        return (((int32_t)codepoint.b[0] & 0x1f) << 6) | ((int32_t)codepoint.b[1] & 0x3f);
    } else if ((codepoint.b[0] & 0xf0) == 0xe0) {
        return (((int32_t)codepoint.b[0] & 0xf) << 12) | (((int32_t)codepoint.b[1] & 0x3f) << 6)
                | ((int32_t)codepoint.b[2] & 0x3f);
    } else {
        return (((int32_t)codepoint.b[0] & 0x7) << 18) | (((int32_t)codepoint.b[1] & 0x3f) << 12)
                | (((int32_t)codepoint.b[2] & 0x3f) << 6) | ((int32_t)codepoint.b[3] & 0x3f);
    }
}

JK_PUBLIC b32 jk_utf8_byte_is_continuation(char byte)
{
    return (byte & 0xc0) == 0x80;
}

JK_PUBLIC JkUtf8CodepointGetResult jk_utf8_codepoint_get(
        JkBuffer buffer, uint64_t *pos, JkUtf8Codepoint *codepoint)
{
    *codepoint = (JkUtf8Codepoint){0};
    if (*pos >= buffer.size) {
        return JK_UTF8_CODEPOINT_GET_EOF;
    }
    if (jk_utf8_byte_is_continuation(buffer.data[*pos])) {
        return JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE;
    }
    codepoint->b[0] = buffer.data[(*pos)++];
    for (int i = 1; i < 4 && *pos < buffer.size && jk_utf8_byte_is_continuation(buffer.data[*pos]);
            i++) {
        codepoint->b[i] = buffer.data[(*pos)++];
    }
    return JK_UTF8_CODEPOINT_GET_SUCCESS;
}

// ---- UTF-8 end --------------------------------------------------------------

// ---- Command line arguments parsing begin -----------------------------------

static void jk_argv_swap_to_front(char **argv, char **arg)
{
    for (; arg > argv; arg--) {
        char *tmp = *arg;
        *arg = *(arg - 1);
        *(arg - 1) = tmp;
    }
}

JK_PUBLIC void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result)
{
    b32 options_ended = 0;
    result->operands = &argv[argc];
    result->operand_count = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
            b32 i_plus_one_is_arg = 0;
            if (argv[i][1] == '-') {
                if (argv[i][2] == '\0') { // -- encountered
                    options_ended = 1;
                } else { // Double hyphen option
                    char *name = &argv[i][2];
                    int end = 0;
                    while (name[end] != '=' && name[end] != '\0') {
                        end++;
                    }
                    b32 matched = 0;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (options_in[j].long_name
                                && strncmp(name, options_in[j].long_name, end) == 0) {
                            matched = 1;
                            options_out[j].present = 1;

                            if (options_in[j].arg_name) {
                                if (name[end] == '=') {
                                    if (name[end + 1] != '\0') {
                                        options_out[j].arg = &name[end + 1];
                                    }
                                } else {
                                    i_plus_one_is_arg = 1;
                                    options_out[j].arg = argv[i + 1];
                                }
                            } else {
                                if (name[end] == '=') {
                                    fprintf(stderr,
                                            "%s: Error in '%s': Option does not accept an "
                                            "argument\n",
                                            argv[0],
                                            argv[i]);
                                    result->usage_error = 1;
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%s'\n", argv[0], argv[i]);
                        result->usage_error = 1;
                    }
                }
            } else { // Single-hypen option(s)
                b32 has_argument = 0;
                for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                    b32 matched = 0;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (*c == options_in[j].flag) {
                            matched = 1;
                            options_out[j].present = 1;
                            has_argument = options_in[j].arg_name != NULL;

                            if (has_argument) {
                                options_out[j].arg = ++c;
                                if (options_out[j].arg[0] == '\0') {
                                    i_plus_one_is_arg = 1;
                                    options_out[j].arg = argv[i + 1];
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%c' in '%s'\n", argv[0], *c, argv[i]);
                        result->usage_error = 1;
                        break;
                    }
                }
            }
            if (&argv[i] > result->operands) {
                jk_argv_swap_to_front(result->operands, &argv[i]);
                result->operands++;
            }
            if (i_plus_one_is_arg) {
                if (argv[i + 1]) {
                    i++;
                    if (&argv[i] > result->operands) {
                        jk_argv_swap_to_front(result->operands, &argv[i]);
                        result->operands++;
                    }
                } else {
                    fprintf(stderr,
                            "%s: Option '%s' missing required argument\n",
                            argv[0],
                            argv[i - 1]);
                    result->usage_error = 1;
                }
            }
        } else { // Regular argument
            result->operand_count++;
            if (&argv[i] < result->operands) {
                result->operands = &argv[i];
            }
        }
    }
}

JK_PUBLIC void jk_options_print_help(FILE *file, JkOption *options, int option_count)
{
    fprintf(file, "OPTIONS\n");
    for (int i = 0; i < option_count; i++) {
        if (i != 0) {
            fprintf(file, "\n");
        }
        printf("\t");
        if (options[i].flag) {
            fprintf(file,
                    "-%c%s%s",
                    options[i].flag,
                    options[i].arg_name ? " " : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        if (options[i].long_name) {
            fprintf(file,
                    "%s--%s%s%s",
                    options[i].flag ? ", " : "",
                    options[i].long_name,
                    options[i].arg_name ? "=" : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        fprintf(file, "%s", options[i].description);
    }
}

/**
 * Attempt to parse a positive integer
 *
 * @return The parsed integer if successful, -1 if failed
 */
JK_PUBLIC int jk_parse_positive_integer(char *string)
{
    int multiplier = 1;
    int result = 0;
    for (int i = (int)strlen(string) - 1; i >= 0; i--) {
        if (isdigit(string[i])) {
            result += (string[i] - '0') * multiplier;
            multiplier *= 10;
        } else if (!(string[i] == ',' || string[i] == '\'' || string[i] == '_')) {
            // Error if character is not a digit or one of the permitted separators
            return -1;
        }
    }
    return result;
}

JK_PUBLIC double jk_parse_double(JkBuffer number_string)
{
    double significand_sign = 1.0;
    double significand = 0.0;
    double exponent_sign = 1.0;
    double exponent = 0.0;

    uint64_t pos = 0;
    int c = jk_buffer_character_next(number_string, &pos);

    if (c == '-') {
        significand_sign = -1.0;

        c = jk_buffer_character_next(number_string, &pos);

        if (!isdigit(c)) {
            return NAN;
        }
    }

    // Parse integer
    do {
        significand = (significand * 10.0) + (c - '0');
    } while (isdigit((c = jk_buffer_character_next(number_string, &pos))));

    // Parse fraction if there is one
    if (c == '.') {
        c = jk_buffer_character_next(number_string, &pos);

        if (!isdigit(c)) {
            return NAN;
        }

        double multiplier = 0.1;
        do {
            significand += (c - '0') * multiplier;
            multiplier /= 10.0;
        } while (isdigit((c = jk_buffer_character_next(number_string, &pos))));
    }

    // Parse exponent if there is one
    if (c == 'e' || c == 'E') {
        c = jk_buffer_character_next(number_string, &pos);

        if ((c == '-' || c == '+')) {
            if (c == '-') {
                exponent_sign = -1.0;
            }
            c = jk_buffer_character_next(number_string, &pos);
        }

        if (!isdigit(c)) {
            return NAN;
        }

        do {
            exponent = (exponent * 10.0) + (c - '0');
        } while (isdigit((c = jk_buffer_character_next(number_string, &pos))));
    }

    return significand_sign * significand * pow(10.0, exponent_sign * exponent);
}

// ---- Command line arguments parsing end -------------------------------------

// ---- Quicksort begin --------------------------------------------------------

static void jk_bytes_swap(void *a, void *b, size_t element_size, void *tmp)
{
    memcpy(tmp, a, element_size);
    memcpy(a, b, element_size);
    memcpy(b, tmp, element_size);
}

/**
 * @brief Sorts elements in the given array.
 *
 * See the definition of quicksort_ints or quicksort_strings for example usage.
 *
 * @param array pointer to the start of the array
 * @param element_count number of elements in the array
 * @param element_size size of a single element in bytes
 * @param tmp pointer to temporary storage the size of a single element
 * @param compare Function used to compare two elements given pointers to them.
 *     Return less than zero if a should come before b, greater than zero if a
 *     should come after b, and zero if they are equal.
 */
JK_PUBLIC void jk_quicksort(void *array_void,
        size_t element_count,
        size_t element_size,
        void *tmp,
        int (*compare)(void *a, void *b))
{
    if (element_count < 2) {
        return;
    }

    char *array = array_void;
    char *low = array;
    char *mid = array + element_size;
    char *high = array + (element_count - 1) * element_size;

    // Move random element to start to use as pivot
    char *random_element = array + (rand() % element_count) * element_size;
    jk_bytes_swap(array, random_element, element_size, tmp);

    while (mid <= high) {
        // Compare mid with pivot. Pivot is always 1 element before mid.
        int comparison = compare(mid, mid - element_size);

        if (comparison < 0) {
            jk_bytes_swap(low, mid, element_size, tmp);
            low += element_size;
            mid += element_size;
        } else if (comparison > 0) {
            jk_bytes_swap(mid, high, element_size, tmp);
            high -= element_size;
        } else {
            mid += element_size;
        }
    }

    size_t left_count = (size_t)(low - array) / element_size;
    size_t right_count = element_count - (size_t)(mid - array) / element_size;
    jk_quicksort(array, left_count, element_size, tmp, compare);
    jk_quicksort(mid, right_count, element_size, tmp, compare);
}

static int jk_int_compare(void *a, void *b)
{
    return *(int *)a - *(int *)b;
}

JK_PUBLIC void jk_quicksort_ints(int *array, int length)
{
    int tmp;
    jk_quicksort(array, length, sizeof(int), &tmp, jk_int_compare);
}

static int jk_float_compare(void *a, void *b)
{
    float delta = *(float *)a - *(float *)b;
    return delta == 0.0f ? 0 : (delta < 0.0f ? -1 : 1);
}

JK_PUBLIC void jk_quicksort_floats(float *array, int length)
{
    float tmp;
    jk_quicksort(array, length, sizeof(float), &tmp, jk_float_compare);
}

static int jk_string_compare(void *a, void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

JK_PUBLIC void jk_quicksort_strings(char **array, int length)
{
    char *tmp;
    jk_quicksort(array, length, sizeof(char *), &tmp, jk_string_compare);
}

// ---- Quicksort end ----------------------------------------------------------

// ---- JkIntVector2 begin -------------------------------------------------------

JK_PUBLIC b32 jk_int_vector_2_equal(JkIntVector2 a, JkIntVector2 b)
{
    return a.x == b.x && a.y == b.y;
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_add(JkIntVector2 a, JkIntVector2 b)
{
    return (JkIntVector2){.x = a.x + b.x, .y = a.y + b.y};
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_sub(JkIntVector2 a, JkIntVector2 b)
{
    return (JkIntVector2){.x = a.x - b.x, .y = a.y - b.y};
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_mul(int32_t scalar, JkIntVector2 vector)
{
    return (JkIntVector2){.x = scalar * vector.x, .y = scalar * vector.y};
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_div(int32_t divisor, JkIntVector2 vector)
{
    return (JkIntVector2){.x = vector.x / divisor, .y = vector.y / divisor};
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_remainder(int32_t divisor, JkIntVector2 vector)
{
    return (JkIntVector2){.x = vector.x % divisor, .y = vector.y % divisor};
}

// ---- JkIntVector2 end -------------------------------------------------------

// ---- JkVector2 begin --------------------------------------------------------

JK_PUBLIC b32 jk_vector_2_approx_equal(JkVector2 a, JkVector2 b, float tolerance)
{
    return jk_float32_equal(a.x, b.x, tolerance) && jk_float32_equal(a.y, b.y, tolerance);
}

JK_PUBLIC JkVector2 jk_vector_2_add(JkVector2 a, JkVector2 b)
{
    return (JkVector2){.x = a.x + b.x, .y = a.y + b.y};
}

JK_PUBLIC JkVector2 jk_vector_2_sub(JkVector2 a, JkVector2 b)
{
    return (JkVector2){.x = a.x - b.x, .y = a.y - b.y};
}

JK_PUBLIC JkVector2 jk_vector_2_mul(float scalar, JkVector2 vector)
{
    return (JkVector2){.x = scalar * vector.x, .y = scalar * vector.y};
}

JK_PUBLIC float jk_vector_2_magnitude_sqr(JkVector2 v)
{
    return v.x * v.x + v.y * v.y;
}

JK_PUBLIC float jk_vector_2_magnitude(JkVector2 v)
{
    return sqrtf(jk_vector_2_magnitude_sqr(v));
}

JK_PUBLIC JkVector2 jk_vector_2_normalized(JkVector2 v)
{
    return jk_vector_2_mul(1.0f / jk_vector_2_magnitude(v), v);
}

JK_PUBLIC float jk_vector_2_dot(JkVector2 u, JkVector2 v)
{
    return u.x * v.x + u.y * v.y;
}

JK_PUBLIC float jk_vector_2_angle_between(JkVector2 u, JkVector2 v)
{
    float sign = u.x * v.y - u.y * v.x < 0.0f ? -1.0f : 1.0f;
    return sign
            * acosf(jk_vector_2_dot(u, v) / (jk_vector_2_magnitude(u) * jk_vector_2_magnitude(v)));
}

JK_PUBLIC JkVector2 jk_vector_2_lerp(JkVector2 a, JkVector2 b, float t)
{
    return jk_vector_2_add(jk_vector_2_mul(1.0f - t, a), jk_vector_2_mul(t, b));
}

JK_PUBLIC float jk_vector_2_distance_squared(JkVector2 a, JkVector2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return dx * dx + dy * dy;
}

JK_PUBLIC JkVector2 jk_vector_2_from_int(JkIntVector2 int_vector)
{
    return (JkVector2){(float)int_vector.x, (float)int_vector.y};
}

JK_PUBLIC JkIntVector2 jk_vector_2_round(JkVector2 vector)
{
    return (JkIntVector2){jk_round(vector.x), jk_round(vector.y)};
}

JK_PUBLIC JkVector2 jk_matrix_2x2_multiply_vector(float matrix[2][2], JkVector2 vector)
{
    return (JkVector2){
        matrix[0][0] * vector.x + matrix[0][1] * vector.y,
        matrix[1][0] * vector.x + matrix[1][1] * vector.y,
    };
}

// ---- JkVector2 end ----------------------------------------------------------

JK_PUBLIC void jk_assert(char *message, char *file, int64_t line)
{
    fprintf(stderr, "Assertion failed: %s, %s:%lld\n", message, file, (long long)line);
    abort();
}

/**
 * @brief Returns a hash for the given 32 bit value
 *
 * From https://github.com/skeeto/hash-prospector
 */
JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x21f0aaad;
    x ^= x >> 15;
    x *= 0x735a2d97;
    x ^= x >> 15;
    return x;
}

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVector2 point)
{
    JkIntVector2 delta = jk_int_vector_2_sub(point, rect.position);
    return 0 <= delta.x && delta.x < rect.dimensions.x && 0 <= delta.y
            && delta.y < rect.dimensions.y;
}

JK_PUBLIC b32 jk_is_power_of_two(uint64_t x)
{
    return x && (x & (x - 1)) == 0;
}

// Rounds up to nearest power of 2. Leaves 0 as 0.
JK_PUBLIC uint64_t jk_round_up_to_power_of_2(uint64_t x)
{
    if (x) {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        x++;
    }
    return x;
}

// Rounds down to nearest power of 2. Leaves 0 as 0.
JK_PUBLIC uint64_t jk_round_down_to_power_of_2(uint64_t x)
{
    if (x) {
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        x -= x >> 1;
    }
    return x;
}

JK_PUBLIC int32_t jk_round(float value)
{
    return (int32_t)(value + 0.5f);
}

JK_PUBLIC float jk_abs(float value)
{
    return value < 0.0f ? -value : value;
}

JK_PUBLIC double jk_abs_64(double value)
{
    return value < 0.0f ? -value : value;
}

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance)
{
    return jk_abs(b - a) < tolerance;
}

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance)
{
    return jk_abs_64(b - a) < tolerance;
}

JK_PUBLIC void jk_print_bytes_uint64(FILE *file, char *format, uint64_t byte_count)
{
    if (byte_count < 1024) {
        fprintf(file, "%llu bytes", (long long)byte_count);
    } else if (byte_count < 1024 * 1024) {
        fprintf(file, format, (double)byte_count / 1024.0);
        fprintf(file, " KiB");
    } else if (byte_count < 1024 * 1024 * 1024) {
        fprintf(file, format, (double)byte_count / (1024.0 * 1024.0));
        fprintf(file, " MiB");
    } else {
        fprintf(file, format, (double)byte_count / (1024.0 * 1024.0 * 1024.0));
        fprintf(file, " GiB");
    }
}

JK_PUBLIC void jk_print_bytes_double(FILE *file, char *format, double byte_count)
{
    if (byte_count < 1024.0) {
        fprintf(file, "%.0f bytes", byte_count);
    } else if (byte_count < 1024.0 * 1024.0) {
        fprintf(file, format, byte_count / 1024.0);
        fprintf(file, " KiB");
    } else if (byte_count < 1024.0 * 1024.0 * 1024.0) {
        fprintf(file, format, byte_count / (1024.0 * 1024.0));
        fprintf(file, " MiB");
    } else {
        fprintf(file, format, byte_count / (1024.0 * 1024.0 * 1024.0));
        fprintf(file, " GiB");
    }
}
