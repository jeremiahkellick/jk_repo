#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "jk_lib.h"

// ---- Buffer begin -----------------------------------------------------------

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string)
{
    JkBuffer buffer = {.size = strlen(string), .data = (uint8_t *)string};
    return buffer;
}

JK_PUBLIC int jk_buffer_character_peek(JkBufferPointer *pointer)
{
    return pointer->index < pointer->buffer.size ? pointer->buffer.data[pointer->index] : EOF;
}

JK_PUBLIC int jk_buffer_character_next(JkBufferPointer *pointer)
{
    int c = jk_buffer_character_peek(pointer);
    pointer->index++;
    return c;
}

// ---- Buffer end -------------------------------------------------------------

// ---- UTF-8 begin ------------------------------------------------------------

JK_PUBLIC void jk_utf8_codepoint_encode(uint32_t codepoint32, JkUtf8Codepoint *codepoint)
{
    if (codepoint32 < 0x80) {
        codepoint->b[0] = (unsigned char)codepoint32;
    } else if (codepoint32 < 0x800) {
        codepoint->b[0] = (unsigned char)(0xc0 | (codepoint32 >> 6));
        codepoint->b[1] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else if (codepoint32 < 0x10000) {
        codepoint->b[0] = (unsigned char)(0xe0 | (codepoint32 >> 12));
        codepoint->b[1] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        codepoint->b[2] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else {
        codepoint->b[0] = (unsigned char)(0xf0 | (codepoint32 >> 18));
        codepoint->b[1] = (unsigned char)(0x80 | ((codepoint32 >> 12) & 0x3f));
        codepoint->b[2] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        codepoint->b[3] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    }
}

JK_PUBLIC b32 jk_utf8_byte_is_continuation(char byte)
{
    return (byte & 0xc0) == 0x80;
}

JK_PUBLIC JkUtf8CodepointGetResult jk_utf8_codepoint_get(
        JkBufferPointer *cursor, JkUtf8Codepoint *codepoint)
{
    if (cursor->index >= cursor->buffer.size) {
        return JK_UTF8_CODEPOINT_GET_EOF;
    }
    if (jk_utf8_byte_is_continuation(cursor->buffer.data[cursor->index])) {
        return JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE;
    }
    codepoint->b[0] = cursor->buffer.data[cursor->index++];
    int i;
    for (i = 0; i < 3 && cursor->index < cursor->buffer.size
            && jk_utf8_byte_is_continuation(cursor->buffer.data[cursor->index]);
            i++) {
        codepoint->b[0] = cursor->buffer.data[cursor->index++];
    }
    if (i < 4) {
        codepoint->b[i] = '\0';
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
    return (JkIntVector2){.x = divisor * vector.x, .y = divisor * vector.y};
}

JK_PUBLIC JkIntVector2 jk_int_vector_2_remainder(int32_t divisor, JkIntVector2 vector)
{
    return (JkIntVector2){.x = vector.x % divisor, .y = vector.y % divisor};
}

// ---- JkIntVector2 end -------------------------------------------------------

// ---- JkVector2 begin --------------------------------------------------------

JK_PUBLIC b32 jk_vector_2_approx_equal(JkVector2 a, JkVector2 b, float tolerance)
{
    return a.x == b.x && a.y == b.y;
}

JK_PUBLIC JkVector2 jk_vector_2_add(JkVector2 a, JkVector2 b)
{
    return (JkVector2){.x = a.x + b.x, .y = a.y + b.y};
}

JK_PUBLIC JkVector2 jk_vector_2_mul(float scalar, JkVector2 vector)
{
    return (JkVector2){.x = scalar * vector.x, .y = scalar * vector.y};
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
    x *= 0xd35a2d97;
    x ^= x >> 15;
    return x;
}

JK_PUBLIC b32 jk_is_power_of_two(uint64_t x)
{
    return x && (x & (x - 1)) == 0;
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
