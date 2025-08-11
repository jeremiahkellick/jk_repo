#ifndef JK_LIB_H
#define JK_LIB_H

#include <stdint.h>

typedef uint32_t b32;

// Can't use an enum because we use these a lot in #if's
#define JK_DEBUG_SLOW 0
#define JK_DEBUG_FAST 1
#define JK_RELEASE 2

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

JK_PUBLIC void jk_buffer_reverse(JkBuffer buffer);

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC int jk_buffer_character_get(JkBuffer buffer, uint64_t pos);

JK_PUBLIC int jk_buffer_character_next(JkBuffer buffer, uint64_t *pos);

JK_PUBLIC int jk_buffer_compare(JkBuffer a, JkBuffer b);

JK_PUBLIC b32 jk_char_is_whitespace(int c);

JK_PUBLIC b32 jk_char_is_digit(int c);

JK_PUBLIC int jk_char_to_lower(int c);

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string);

JK_PUBLIC int64_t jk_string_find(JkBuffer string, JkBuffer substring);

typedef enum JkFormatItemType {
    JK_FORMAT_ITEM_NULL_TERMINATED,
    JK_FORMAT_ITEM_STRING,
    JK_FORMAT_ITEM_INT,
    JK_FORMAT_ITEM_UNSIGNED,
    JK_FORMAT_ITEM_HEX,
    JK_FORMAT_ITEM_BINARY,

    JK_FORMAT_ITEM_TYPE_COUNT,
} JkFormatItemType;

typedef struct JkFormatItem {
    JkFormatItemType type;
    union {
        char *null_terminated;
        JkBuffer string;
        int64_t signed_value;
        uint64_t unsigned_value;
    };
    int16_t min_width;
} JkFormatItem;

typedef struct JkFormatItemArray {
    uint64_t count;
    JkFormatItem *items;
} JkFormatItemArray;

JK_PUBLIC JkFormatItem jkfn(char *null_termianted);

JK_PUBLIC JkFormatItem jkfs(JkBuffer string);

JK_PUBLIC JkFormatItem jkfi(int64_t signed_value);

JK_PUBLIC JkFormatItem jkfu(uint64_t unsigned_value);

JK_PUBLIC JkFormatItem jkfh(uint64_t hex_value, int16_t min_width);

JK_PUBLIC JkFormatItem jkfb(uint64_t binary_value, int16_t min_width);

// ---- Buffer end -------------------------------------------------------------

// ---- Math begin -------------------------------------------------------------

JK_PUBLIC float jk_round_f32(float value);

JK_PUBLIC float jk_floor_f32(float value);

JK_PUBLIC float jk_ceil_f32(float value);

JK_PUBLIC float jk_remainder_f32(float x, float y);

JK_PUBLIC float jk_sqrt_f32(float value);

JK_PUBLIC float jk_sin_core_f32(float value);

JK_PUBLIC float jk_sin_f32(float value);

JK_PUBLIC float jk_cos_f32(float value);

JK_PUBLIC float jk_acos_core_f32(float value);

JK_PUBLIC float jk_acos_f32(float value);

// ---- Math end ---------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

typedef struct JkArenaRoot {
    JkBuffer memory;
} JkArenaRoot;

typedef struct JkArena {
    uint64_t base;
    uint64_t pos;
    JkArenaRoot *root;
    b32 (*grow)(struct JkArena *arena, uint64_t new_size);
} JkArena;

JK_PUBLIC JkArena jk_arena_fixed_init(JkArenaRoot *root, JkBuffer memory);

JK_PUBLIC b32 jk_arena_valid(JkArena *arena);

JK_PUBLIC void *jk_arena_push(JkArena *arena, uint64_t size);

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, uint64_t size);

JK_PUBLIC void jk_arena_pop(JkArena *arena, uint64_t size);

JK_PUBLIC JkArena jk_arena_child_get(JkArena *parent);

JK_PUBLIC void *jk_arena_pointer_current(JkArena *arena);

// ---- Arena end --------------------------------------------------------------

// These things conceptually belong to the buffer code but depend on the JkArena declaration so
// we'll just let them hang out down here I guess...

JK_PUBLIC JkBuffer jk_buffer_copy(JkArena *arena, JkBuffer buffer);

JK_PUBLIC char *jk_buffer_to_null_terminated(JkArena *arena, JkBuffer buffer);

JK_PUBLIC JkBuffer jk_int_to_string(JkArena *arena, int64_t value);

JK_PUBLIC JkBuffer jk_unsigned_to_string(JkArena *arena, uint64_t value);

JK_PUBLIC JkBuffer jk_unsigned_to_hexadecimal_string(
        JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkBuffer jk_unsigned_to_binary_string(JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkFormatItem jkf_nl;

JK_PUBLIC JkBuffer jk_format(JkArena *arena, JkFormatItemArray items);

#define JK_FORMAT(arena, ...)                                                          \
    jk_format(arena,                                                                   \
            (JkFormatItemArray){                                                       \
                .count = sizeof((JkFormatItem[]){__VA_ARGS__}) / sizeof(JkFormatItem), \
                .items = (JkFormatItem[]){__VA_ARGS__}})

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

// ---- Quicksort begin --------------------------------------------------------

JK_PUBLIC void jk_quicksort(void *array,
        uint64_t element_count,
        uint64_t element_size,
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

// ---- Random generator begin -------------------------------------------------

typedef struct JkRandomGeneratorU64 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
} JkRandomGeneratorU64;

JK_PUBLIC JkRandomGeneratorU64 jk_random_generator_new_u64(uint64_t seed);

JK_PUBLIC uint64_t jk_random_u64(JkRandomGeneratorU64 *g);

// ---- Random generator end ---------------------------------------------------

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

JK_PUBLIC void jk_assert_failed(char *message, char *file, int64_t line);

#define JK_ASSERT(expression) \
    (void)((!!(expression)) || (jk_assert_failed(#expression, __FILE__, (int64_t)(__LINE__)), 0))

#if JK_BUILD_MODE == JK_RELEASE
#define JK_DEBUG_ASSERT(...)
#else
#define JK_DEBUG_ASSERT(expression) JK_ASSERT(expression)
#endif

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define JK_DATA_GET(pointer, index, type) (*(type *)((uint8_t *)(pointer) + (index) * sizeof(type)))

#define JK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define JK_MAX(a, b) ((a) < (b) ? (b) : (a))

#define JK_CLAMP(v, min, max) ((v) < (min) ? (min) : ((max) < (v) ? (max) : (v)))

#define JK_ABS(value) ((value) < 0 ? -(value) : (value))

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

#define JK_EOF (-1)

JK_PUBLIC int jk_parse_positive_integer(char *string);

JK_PUBLIC void jk_memset(void *address, uint8_t value, uint64_t size);

JK_PUBLIC void jk_memcpy(void *dest, void *src, uint64_t size);

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVector2 point);

JK_PUBLIC uint64_t jk_count_leading_zeros(uint64_t value);

JK_PUBLIC b32 jk_is_power_of_two(uint64_t x);

JK_PUBLIC uint64_t jk_round_up_to_power_of_2(uint64_t x);

JK_PUBLIC uint64_t jk_round_down_to_power_of_2(uint64_t x);

JK_PUBLIC int32_t jk_round(float value);

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance);

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance);

JK_PUBLIC uint64_t jk_platform_page_size_round_up(uint64_t n);

JK_PUBLIC uint64_t jk_platform_page_size_round_down(uint64_t n);

#endif
