#ifndef JK_LIB_H
#define JK_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t b32;

// ---- Buffer begin -----------------------------------------------------------

typedef struct JkBuffer {
    size_t size;
    uint8_t *data;
} JkBuffer;

typedef struct JkBufferPointer {
    JkBuffer buffer;
    size_t index;
} JkBufferPointer;

#define JK_STRING(string_literal) \
    ((JkBuffer){sizeof(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC int jk_buffer_character_peek(JkBufferPointer *pointer);

JK_PUBLIC int jk_buffer_character_next(JkBufferPointer *pointer);

// ---- Buffer end -------------------------------------------------------------

// ---- UTF-8 begin ------------------------------------------------------------

typedef struct JkUtf8Codepoint {
    uint8_t b[4];
} JkUtf8Codepoint;

JK_PUBLIC void jk_utf8_codepoint_encode(uint32_t codepoint32, JkUtf8Codepoint *codepoint);

JK_PUBLIC b32 jk_utf8_byte_is_continuation(char byte);

typedef enum JkUtf8CodepointGetResult {
    JK_UTF8_CODEPOINT_GET_SUCCESS,
    JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE,
    JK_UTF8_CODEPOINT_GET_EOF,
} JkUtf8CodepointGetResult;

JK_PUBLIC JkUtf8CodepointGetResult jk_utf8_codepoint_get(
        JkBufferPointer *cursor, JkUtf8Codepoint *codepoint);

// ---- UTF-8 end --------------------------------------------------------------

// ---- Command line arguments parsing begin -----------------------------------

typedef struct JkOption {
    /**
     * Character used as the short-option flag. The null character means there is no short form of
     * this option. An option must have some way to refer to it. If this is the null character,
     * long_name must not be null.
     */
    char flag;

    /**
     * The long name of this option. NULL means there is no long name for this option. An option
     * must have some way to refer to it. If this is NULL, flag must not be the null character.
     */
    char *long_name;

    /** Name of the argument for this option. NULL if this option does not accept an argument. */
    char *arg_name;

    /** Description of this option used to print help text */
    char *description;
} JkOption;

typedef struct JkOptionResult {
    b32 present;
    char *arg;
} JkOptionResult;

typedef struct JkOptionsParseResult {
    /** Pointer to the first operand (first non-option argument) */
    char **operands;
    size_t operand_count;
    b32 usage_error;
} JkOptionsParseResult;

JK_PUBLIC void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result);

JK_PUBLIC void jk_options_print_help(FILE *file, JkOption *options, int option_count);

JK_PUBLIC int jk_parse_positive_integer(char *string);

// ---- Command line arguments parsing end -------------------------------------

// ---- Quicksort begin --------------------------------------------------------

JK_PUBLIC void jk_quicksort(void *array,
        size_t element_count,
        size_t element_size,
        void *tmp,
        int (*compare)(void *a, void *b));

JK_PUBLIC void jk_quicksort_ints(int *array, int length);

JK_PUBLIC void jk_quicksort_floats(float *array, int length);

JK_PUBLIC void jk_quicksort_strings(char **array, int length);

// ---- Quicksort end ----------------------------------------------------------

// ---- JkIntVector2 begin -------------------------------------------------------

typedef union JkIntVector2 {
    int32_t coords[2];
    struct {
        int32_t x;
        int32_t y;
    };
} JkIntVector2;

JK_PUBLIC b32 jk_int_vector_2_equal(JkIntVector2 a, JkIntVector2 b);

JK_PUBLIC JkIntVector2 jk_int_vector_2_add(JkIntVector2 a, JkIntVector2 b);

JK_PUBLIC JkIntVector2 jk_int_vector_2_sub(JkIntVector2 a, JkIntVector2 b);

JK_PUBLIC JkIntVector2 jk_int_vector_2_mul(int32_t scalar, JkIntVector2 vector);

JK_PUBLIC JkIntVector2 jk_int_vector_2_div(int32_t divisor, JkIntVector2 vector);

JK_PUBLIC JkIntVector2 jk_int_vector_2_remainder(int32_t divisor, JkIntVector2 vector);

// ---- IntVector2 end ---------------------------------------------------------

// ---- JkVector2 begin -------------------------------------------------------

typedef union JkVector2 {
    float coords[2];
    struct {
        float x;
        float y;
    };
} JkVector2;

JK_PUBLIC b32 jk_vector_2_approx_equal(JkVector2 a, JkVector2 b, float tolerance);

JK_PUBLIC JkVector2 jk_vector_2_add(JkVector2 a, JkVector2 b);

JK_PUBLIC JkVector2 jk_vector_2_mul(float scalar, JkVector2 vector);

// ---- IntVector2 end ---------------------------------------------------------

JK_PUBLIC void jk_assert(char *message, char *file, int64_t line);

#define JK_ASSERT(expression) \
    (void)((!!(expression)) || (jk_assert(#expression, __FILE__, (int64_t)(__LINE__)), 0))

#ifdef NDEBUG
#define JK_DEBUG_ASSERT(...)
#else
#define JK_DEBUG_ASSERT(expression) JK_ASSERT(expression)
#endif

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define JK_DATA_GET(pointer, index, type) (*(type *)((uint8_t *)(pointer) + (index) * sizeof(type)))

#define JK_MIN(a, b) (a < b ? a : b)
#define JK_MAX(a, b) (a < b ? b : a)

#define JK_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC b32 jk_is_power_of_two(uint64_t x);

JK_PUBLIC size_t jk_platform_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_platform_page_size_round_down(size_t n);

JK_PUBLIC void jk_print_bytes_uint64(FILE *file, char *format, uint64_t byte_count);

JK_PUBLIC void jk_print_bytes_double(FILE *file, char *format, double byte_count);

#endif
