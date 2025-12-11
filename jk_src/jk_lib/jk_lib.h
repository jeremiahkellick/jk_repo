#ifndef JK_LIB_H
#define JK_LIB_H

#include <stdint.h>

typedef uint32_t b32;

// Can't use an enum because we use these a lot in #if's
#define JK_DEBUG_SLOW 0
#define JK_DEBUG_FAST 1
#define JK_RELEASE 2

#define JK_SIZEOF(type) ((int64_t)sizeof(type))

#if defined(_WIN32)
#define JK_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define JK_NOINLINE __attribute__((noinline))
#else
#define JK_NOINLINE
#endif

// ---- Buffer begin -----------------------------------------------------------

typedef struct JkBuffer {
    int64_t size;
    uint8_t *data;
} JkBuffer;

typedef struct JkBufferArray {
    int64_t count;
    JkBuffer *items;
} JkBufferArray;

typedef struct JkSpan {
    int64_t size;
    int64_t offset;
} JkSpan;

#define JK_STRING(string_literal) \
    ((JkBuffer){JK_SIZEOF(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

#define JK_STRING_INITIALIZER(string_literal)                    \
    {                                                            \
        JK_SIZEOF(string_literal) - 1, (uint8_t *)string_literal \
    }

#define JKSI JK_STRING_INITIALIZER

#define JK_BUFFER_INIT_FROM_BYTE_ARRAY(byte_array)        \
    {                                                     \
        .size = JK_SIZEOF(byte_array), .data = byte_array \
    }

JK_PUBLIC void jk_buffer_zero(JkBuffer buffer);

JK_PUBLIC void jk_buffer_reverse(JkBuffer buffer);

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC int32_t jk_buffer_character_get(JkBuffer buffer, int64_t pos);

JK_PUBLIC int32_t jk_buffer_character_next(JkBuffer buffer, int64_t *pos);

JK_PUBLIC JkBuffer jk_buffer_null_terminated_next(JkBuffer buffer, int64_t *pos);

JK_PUBLIC uint32_t jk_buffer_bits_peek(JkBuffer buffer, int64_t bit_cursor, int64_t bit_count);

JK_PUBLIC uint32_t jk_buffer_bits_read(JkBuffer buffer, int64_t *bit_cursor, int64_t bit_count);

#define JK_BUFFER_FIELD_NEXT(buffer, pos, type) \
    (*(pos) += JK_SIZEOF(type),                 \
            (*(pos) < (buffer).size ? (type *)((buffer).data + (*(pos) - JK_SIZEOF(type))) : 0))

#define JK_BUFFER_FIELD_READ(buffer, pos, type, default)                                    \
    (*(pos) += JK_SIZEOF(type),                                                             \
            (*(pos) < (buffer).size ? *(type *)((buffer).data + (*(pos) - JK_SIZEOF(type))) \
                                    : (default)))

JK_PUBLIC int32_t jk_buffer_compare(JkBuffer a, JkBuffer b);

JK_PUBLIC b32 jk_char_is_whitespace(int32_t c);

JK_PUBLIC b32 jk_char_is_digit(int32_t c);

JK_PUBLIC int32_t jk_char_to_lower(int32_t c);

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string);

JK_PUBLIC int64_t jk_string_find(JkBuffer string, JkBuffer substring);

typedef enum JkFormatItemType {
    JK_FORMAT_ITEM_NULL_TERMINATED,
    JK_FORMAT_ITEM_STRING,
    JK_FORMAT_ITEM_INT,
    JK_FORMAT_ITEM_UNSIGNED,
    JK_FORMAT_ITEM_HEX,
    JK_FORMAT_ITEM_BINARY,
    JK_FORMAT_ITEM_FLOAT,

    JK_FORMAT_ITEM_TYPE_COUNT,
} JkFormatItemType;

typedef struct JkFormatItem {
    JkFormatItemType type;
    union {
        char *null_terminated;
        JkBuffer string;
        int64_t signed_value;
        uint64_t unsigned_value;
        double float_value;
    };
    int16_t param;
} JkFormatItem;

typedef struct JkFormatItemArray {
    int64_t count;
    JkFormatItem *items;
} JkFormatItemArray;

JK_PUBLIC JkFormatItem jkfn(char *null_termianted);

JK_PUBLIC JkFormatItem jkfs(JkBuffer string);

JK_PUBLIC JkFormatItem jkfi(int64_t signed_value);

JK_PUBLIC JkFormatItem jkfu(uint64_t unsigned_value);

JK_PUBLIC JkFormatItem jkfuw(uint64_t unsigned_value, int16_t min_width);

JK_PUBLIC JkFormatItem jkfh(uint64_t hex_value, int16_t min_width);

JK_PUBLIC JkFormatItem jkfb(uint64_t binary_value, int16_t min_width);

JK_PUBLIC JkFormatItem jkff(double float_value, int16_t decimal_places);

JK_PUBLIC void (*jk_print)(JkBuffer string);

// ---- Buffer end -------------------------------------------------------------

// ---- Math begin -------------------------------------------------------------

#define JK_FLOAT_EXPONENT_SPECIAL INT32_MAX

typedef struct JkFloatUnpacked {
    b32 sign;

    // Note the exponent and signficand work differently here than what you are probably used to
    // with floating point formats. Generally, the significand is thought of as a fixed-point number
    // with one bit to the left of the binary decimal and all the other bits to the right.
    // For example, 1.001b * 2^e. (I'm using b as a suffix to denote binary numbers).
    //
    // Now notice how we can write 1.001b as an integer multiplied by 2 raised to some power
    // 1.001b = 1001b * 2^-3
    // Inserting this back into the original expression we get the following
    // 1.001b * 2^e = 1001b * 2^-3 * 2^e = 1001b * 2^(e-3)
    //
    // So we can use an integer significand, as long as we use an exponent that's 3 less than the
    // traditional floating-point exponent. Why 3? Because in our example that was the bit-width of
    // the fractional portion of the significand (the mantissa).
    //
    // In this struct, "exponent" refers to this reduced exponent. If e is the traditional exponent,
    // then exponent = e - bit_width_of_mantissa. And "significand" refers to the significant bits
    // of the floating point value, including the leading bit (which is generally only implied in
    // the binary representation). However, there's no implied binary point in the significand. We
    // interpret it as an unsigned integer. Then the value represented by some JkFloatUnpacked f
    // is (f.sign ? -1 : 1) * f.significand * pow(2, f.exponent)
    //
    // Also note that in this scheme, the number of bits in the significand that will represent a
    // fractional quantity in the final value is simply -exponent

    int32_t exponent;
    uint64_t significand;
} JkFloatUnpacked;

JK_PUBLIC JkFloatUnpacked jk_unpack_f64(double value);

JK_PUBLIC double jk_pack_f64(JkFloatUnpacked f);

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
    int64_t base;
    int64_t pos;
    JkArenaRoot *root;
    b32 (*grow)(struct JkArena *arena, int64_t new_size);
} JkArena;

JK_PUBLIC JkArena jk_arena_fixed_init(JkArenaRoot *root, JkBuffer memory);

JK_PUBLIC b32 jk_arena_valid(JkArena *arena);

JK_PUBLIC void *jk_arena_push(JkArena *arena, int64_t size);

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_arena_push_buffer(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_arena_push_buffer_zero(JkArena *arena, int64_t size);

#define JK_ARENA_PUSH_ARRAY(arena, array, item_count)                                   \
    do {                                                                                \
        (array).count = item_count;                                                     \
        (array).items = jk_arena_push(arena, (item_count) * JK_SIZEOF(*(array).items)); \
    } while (0)

JK_PUBLIC void jk_arena_pop(JkArena *arena, int64_t size);

JK_PUBLIC JkArena jk_arena_child_get(JkArena *parent);

JK_PUBLIC void *jk_arena_pointer_current(JkArena *arena);

// ---- Arena end --------------------------------------------------------------

// These things conceptually belong to the buffer code but depend on the JkArena declaration so
// we'll just let them hang out down here I guess...

JK_PUBLIC JkBuffer jk_buffer_copy(JkArena *arena, JkBuffer buffer);

JK_PUBLIC JkBuffer jk_buffer_alloc(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_buffer_alloc_zero(JkArena *arena, int64_t size);

JK_PUBLIC int64_t jk_strlen(char *string);

JK_PUBLIC char *jk_buffer_to_null_terminated(JkArena *arena, JkBuffer buffer);

JK_PUBLIC JkBuffer jk_int_to_string(JkArena *arena, int64_t value);

JK_PUBLIC JkBuffer jk_unsigned_to_string(JkArena *arena, uint64_t value, int64_t min_width);

JK_PUBLIC JkBuffer jk_unsigned_to_hexadecimal_string(
        JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkBuffer jk_unsigned_to_binary_string(JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkBuffer jk_f64_to_string(JkArena *arena, double value, int64_t decimal_places);

JK_PUBLIC JkFormatItem jkf_nl;

JK_PUBLIC JkBuffer jk_format(JkArena *arena, JkFormatItemArray items);

#define JK_FORMAT(arena, ...)                                                                  \
    jk_format(arena,                                                                           \
            (JkFormatItemArray){                                                               \
                .count = JK_SIZEOF(((JkFormatItem[]){__VA_ARGS__})) / JK_SIZEOF(JkFormatItem), \
                .items = (JkFormatItem[]){__VA_ARGS__}})

JK_PUBLIC void jk_print_fmt(JkArena *arena, JkFormatItemArray items);

#define JK_PRINT_FMT(arena, ...)                                                               \
    jk_print_fmt(arena,                                                                        \
            (JkFormatItemArray){                                                               \
                .count = JK_SIZEOF(((JkFormatItem[]){__VA_ARGS__})) / JK_SIZEOF(JkFormatItem), \
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
        JkBuffer buffer, int64_t *pos, JkUtf8Codepoint *codepoint);

// ---- UTF-8 end --------------------------------------------------------------

// ---- Quicksort begin --------------------------------------------------------

JK_PUBLIC void jk_quicksort(void *array,
        int64_t element_count,
        int64_t element_size,
        void *tmp,
        int32_t (*compare)(void *a, void *b));

JK_PUBLIC void jk_quicksort_ints(int32_t *array, int32_t length);

JK_PUBLIC void jk_quicksort_floats(float *array, int32_t length);

JK_PUBLIC void jk_quicksort_strings(char **array, int32_t length);

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

// ---- JkVector3 begin -------------------------------------------------------

typedef union JkVector3 {
    float coords[3];
    struct {
        float x;
        float y;
        float z;
    };
} JkVector3;

typedef struct JkVector3Array {
    int64_t count;
    JkVector3 *items;
} JkVector3Array;

JK_PUBLIC b32 jk_vector_3_approx_equal(JkVector3 a, JkVector3 b, float tolerance);

JK_PUBLIC JkVector3 jk_vector_3_add(JkVector3 a, JkVector3 b);

JK_PUBLIC JkVector3 jk_vector_3_sub(JkVector3 a, JkVector3 b);

JK_PUBLIC JkVector3 jk_vector_3_mul(float scalar, JkVector3 vector);

JK_PUBLIC JkVector3 jk_vector_3_ceil(JkVector3 v);

JK_PUBLIC float jk_vector_3_magnitude_sqr(JkVector3 v);

JK_PUBLIC float jk_vector_3_magnitude(JkVector3 v);

JK_PUBLIC JkVector3 jk_vector_3_normalized(JkVector3 v);

JK_PUBLIC float jk_vector_3_dot(JkVector3 u, JkVector3 v);

JK_PUBLIC JkVector3 jk_vector_3_cross(JkVector3 u, JkVector3 v);

JK_PUBLIC float jk_vector_3_angle_between(JkVector3 u, JkVector3 v);

JK_PUBLIC JkVector3 jk_vector_3_lerp(JkVector3 a, JkVector3 b, float t);

JK_PUBLIC float jk_vector_3_distance_squared(JkVector3 a, JkVector3 b);

JK_PUBLIC JkVector3 jk_vector_3_round(JkVector3 vector);

JK_PUBLIC JkVector2 jk_vector_3_to_2(JkVector3 v);

// ---- JkVector3 end ----------------------------------------------------------

// ---- Shapes begin -----------------------------------------------------------

typedef union JkSegment {
    JkVector2 endpoints[2];
    struct {
        JkVector2 p1;
        JkVector2 p2;
    };
} JkSegment;

JK_PUBLIC float jk_segment_y_intersection(JkSegment segment, float y);

JK_PUBLIC float jk_segment_x_intersection(JkSegment segment, float x);

typedef struct JkEdge {
    JkSegment segment;
    float direction;
} JkEdge;

typedef struct JkEdgeArray {
    int64_t count;
    JkEdge *items;
} JkEdgeArray;

JK_PUBLIC JkEdge jk_points_to_edge(JkVector2 a, JkVector2 b);

typedef struct JkRect {
    JkVector2 min;
    JkVector2 max;
} JkRect;

typedef struct JkIntRect {
    JkIntVector2 min;
    JkIntVector2 max;
} JkIntRect;

JK_PUBLIC JkRect jk_rect(JkVector2 position, JkVector2 dimensions);

JK_PUBLIC JkIntVector2 jk_int_rect_dimensions(JkIntRect rect);

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVector2 point);

JK_PUBLIC JkIntRect jk_int_rect_intersect(JkIntRect a, JkIntRect b);

typedef struct JkTriangle2 {
    JkVector2 v[3];
} JkTriangle2;

JK_PUBLIC JkIntRect jk_triangle2_int_bounding_box(JkTriangle2 t);

JK_PUBLIC JkEdgeArray jk_triangle2_edges_get(JkArena *arena, JkTriangle2 t);

// ---- Shapes end -------------------------------------------------------------

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

#define JK_KILOBYTE (1ll << 10)
#define JK_MEGABYTE (1ll << 20)
#define JK_GIGABYTE (1ll << 30)

typedef struct JkFloatArray {
    int64_t count;
    float *items;
} JkFloatArray;

typedef struct JkColor {
    union {
        struct {
#if defined(__wasm32__)
            uint8_t r;
            uint8_t g;
            uint8_t b;
#else
            uint8_t b;
            uint8_t g;
            uint8_t r;
#endif
            uint8_t a;
        };
        uint8_t v[4];
    };
} JkColor;

JK_PUBLIC JkColor jk_color_alpha_blend(JkColor foreground, JkColor background, uint8_t alpha);

JK_PUBLIC JkColor jk_color_disjoint_over(JkColor fg, JkColor bg);

JK_PUBLIC void jk_panic(void);

JK_PUBLIC void jk_assert_failed(char *message, char *file, int64_t line);

#define JK_ASSERT(expression) \
    (void)((!!(expression)) || (jk_assert_failed(#expression, __FILE__, (int64_t)(__LINE__)), 0))

#if JK_BUILD_MODE == JK_RELEASE
#define JK_DEBUG_ASSERT(...)
#else
#define JK_DEBUG_ASSERT(expression) JK_ASSERT(expression)
#endif

#define JK_ARRAY_COUNT(array) (JK_SIZEOF(array) / JK_SIZEOF((array)[0]))

#define JK_BUFFER_FROM_ARRAY(array)                    \
    (JkBuffer)                                         \
    {                                                  \
        .size = JK_ARRAY_COUNT(array), .data = (array) \
    }

#define JK_ARRAY_FROM_SPAN(array, base, span)                        \
    do {                                                             \
        (array).count = (span).size / JK_SIZEOF(*(array).items);     \
        (array).items = (void *)((uint8_t *)(base) + (span).offset); \
    } while (0)

#define JK_DATA_GET(pointer, index, type) \
    (*(type *)((uint8_t *)(pointer) + (index) * JK_SIZEOF(type)))

#define JK_FLOOR_DIV(dividend, divisor) \
    (0 <= (dividend) ? (dividend) / (divisor) : ((dividend) - (divisor) + 1) / (divisor))
#define JK_MOD(dividend, divisor) ((((dividend) % (divisor)) + (divisor)) % (divisor))

#define JK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define JK_MAX(a, b) ((a) < (b) ? (b) : (a))

#define JK_MIN3(a, b, c) ((a) < JK_MIN(b, c) ? (a) : JK_MIN(b, c))
#define JK_MAX3(a, b, c) (JK_MAX(a, b) < (c) ? (c) : JK_MAX(a, b))

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

#define JK_FLAG_SET(bitfield, flag, value)                                                \
    do {                                                                                  \
        bitfield = (((bitfield) & ~JK_MASK(flag)) | (((uint64_t)(!!(value))) << (flag))); \
    } while (0)

#define JK_PI 3.14159265358979323846264338327950288419716939937510582097494459230781640628
#define JK_INV_SQRT_2 0.70710678118654752440084

#define JK_OOB (-1)

JK_PUBLIC int32_t jk_parse_positive_integer(char *string);

JK_PUBLIC void *jk_memset(void *address, uint8_t value, int64_t size);

JK_PUBLIC void *jk_memcpy(void *dest, void *src, int64_t size);

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC uint8_t jk_bit_reverse_table[256];

JK_PUBLIC uint16_t jk_bit_reverse_u16(uint16_t value);

JK_PUBLIC int64_t jk_count_leading_zeros(uint64_t value);

JK_PUBLIC uint64_t jk_signed_shift(uint64_t value, int64_t amount);

JK_PUBLIC b32 jk_is_power_of_two(int64_t x);

JK_PUBLIC int64_t jk_round_up_to_power_of_2(int64_t x);

JK_PUBLIC int64_t jk_round_down_to_power_of_2(int64_t x);

JK_PUBLIC int32_t jk_round(float value);

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance);

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance);

#endif
