#ifndef JK_LIB_H
#define JK_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t b32;

// ---- Buffer begin -----------------------------------------------------------

typedef struct JkBuffer {
    uint64_t size;
    uint8_t *data;
} JkBuffer;

typedef struct JkBufferArray {
    uint64_t count;
    JkBuffer *items;
} JkBufferArray;

typedef struct JkSpan {
    uint64_t size;
    uint64_t offset;
} JkSpan;

#define JK_STRING(string_literal) \
    ((JkBuffer){sizeof(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

#define JK_STRING_INITIALIZER(string_literal)                 \
    {                                                         \
        sizeof(string_literal) - 1, (uint8_t *)string_literal \
    }

#define JKSI JK_STRING_INITIALIZER

JK_PUBLIC void jk_buffer_zero(JkBuffer buffer);

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC int jk_buffer_character_get(JkBuffer buffer, uint64_t pos);

JK_PUBLIC int jk_buffer_character_next(JkBuffer buffer, uint64_t *pos);

JK_PUBLIC b32 jk_char_is_whitespace(uint8_t c);

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string);

// ---- Buffer end -------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

typedef struct JkArena {
    JkBuffer memory;
    uint64_t pos;
} JkArena;

JK_PUBLIC void *jk_arena_alloc(JkArena *arena, uint64_t byte_count);

JK_PUBLIC void *jk_arena_alloc_zero(JkArena *arena, uint64_t byte_count);

JK_PUBLIC void *jk_arena_pointer_get(JkArena *arena);

JK_PUBLIC void jk_arena_pointer_set(JkArena *arena, void *pointer);

// ---- Arena end --------------------------------------------------------------

// ---- UTF-8 begin ------------------------------------------------------------

typedef struct JkUtf8Codepoint {
    uint8_t b[4];
} JkUtf8Codepoint;

JK_PUBLIC JkUtf8Codepoint jk_utf8_codepoint_encode(uint32_t codepoint32);

JK_PUBLIC int32_t jk_utf8_codepoint_decode(JkUtf8Codepoint codepoint);

JK_PUBLIC b32 jk_utf8_byte_is_continuation(char byte);

typedef enum JkUtf8CodepointGetResult {
    JK_UTF8_CODEPOINT_GET_SUCCESS,
    JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE,
    JK_UTF8_CODEPOINT_GET_EOF,
} JkUtf8CodepointGetResult;

JK_PUBLIC JkUtf8CodepointGetResult jk_utf8_codepoint_get(
        JkBuffer buffer, uint64_t *pos, JkUtf8Codepoint *codepoint);

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

JK_PUBLIC double jk_parse_double(JkBuffer number_string);

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

JK_PUBLIC JkVector2 jk_vector_2_sub(JkVector2 a, JkVector2 b);

JK_PUBLIC JkVector2 jk_vector_2_mul(float scalar, JkVector2 vector);

JK_PUBLIC JkVector2 jk_vector_2_ceil(JkVector2 v);

JK_PUBLIC JkIntVector2 jk_vector_2_ceil_i(JkVector2 v);

JK_PUBLIC float jk_vector_2_magnitude_sqr(JkVector2 v);

JK_PUBLIC float jk_vector_2_magnitude(JkVector2 v);

JK_PUBLIC JkVector2 jk_vector_2_normalized(JkVector2 v);

JK_PUBLIC float jk_vector_2_dot(JkVector2 u, JkVector2 v);

JK_PUBLIC float jk_vector_2_angle_between(JkVector2 u, JkVector2 v);

JK_PUBLIC JkVector2 jk_vector_2_lerp(JkVector2 a, JkVector2 b, float t);

JK_PUBLIC float jk_vector_2_distance_squared(JkVector2 a, JkVector2 b);

JK_PUBLIC JkVector2 jk_vector_2_from_int(JkIntVector2 int_vector);

JK_PUBLIC JkIntVector2 jk_vector_2_round(JkVector2 vector);

JK_PUBLIC JkVector2 jk_matrix_2x2_multiply_vector(float matrix[2][2], JkVector2 vector);

// ---- JkVector2 end ----------------------------------------------------------

#define JK_KILOBYTE (1llu << 10)
#define JK_MEGABYTE (1llu << 20)
#define JK_GIGABYTE (1llu << 30)

typedef struct JkFloatArray {
    uint64_t count;
    float *items;
} JkFloatArray;

typedef struct JkColor {
    union {
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t a;
        };
        uint8_t v[4];
    };
} JkColor;

typedef struct JkIntRect {
    JkIntVector2 position;
    JkIntVector2 dimensions;
} JkIntRect;

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

#define JK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define JK_MAX(a, b) ((a) < (b) ? (b) : (a))

#define JK_SWAP(a, b, type)   \
    do {                      \
        type jk_swap_tmp = a; \
        a = b;                \
        b = jk_swap_tmp;      \
    } while (0)

#define JK_MASK(index) (1llu << (index))

#define JK_FLAG_GET(bitfield, flag) (((bitfield) >> (flag)) & 1)

#define JK_FLAG_SET(bitfield, flag, value) \
    (((bitfield) & ~JK_MASK(flag)) | (((uint64_t)(!!(value))) << (flag)))

#define JK_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628
#define JK_INV_SQRT_2 0.70710678118654752440084

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVector2 point);

JK_PUBLIC b32 jk_is_power_of_two(uint64_t x);

JK_PUBLIC uint64_t jk_round_up_to_power_of_2(uint64_t x);

JK_PUBLIC uint64_t jk_round_down_to_power_of_2(uint64_t x);

JK_PUBLIC int32_t jk_round(float value);

JK_PUBLIC float jk_abs(float value);

JK_PUBLIC double jk_abs_64(double value);

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance);

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance);

JK_PUBLIC size_t jk_platform_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_platform_page_size_round_down(size_t n);

JK_PUBLIC void jk_print_bytes_uint64(FILE *file, char *format, uint64_t byte_count);

JK_PUBLIC void jk_print_bytes_double(FILE *file, char *format, double byte_count);

#endif
