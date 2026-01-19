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

typedef struct JkBuffer {
    int64_t size;
    uint8_t *data;
} JkBuffer;

typedef union JkVec3 {
    float v[3];
    struct {
        float x;
        float y;
        float z;
    };
} JkVec3;

typedef union JkVec4 {
    float v[4];
    struct {
        float x;
        float y;
        float z;
        float w;
    };
} JkVec4;

typedef struct JkArenaRoot {
    JkBuffer memory;
} JkArenaRoot;

typedef struct JkArena {
    int64_t base;
    int64_t pos;
    JkArenaRoot *root;
    b32 (*grow)(struct JkArena *arena, int64_t new_size);
} JkArena;

// ---- Buffer begin -----------------------------------------------------------

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

JK_PUBLIC JkBuffer jk_buffer_copy(JkArena *arena, JkBuffer buffer);

JK_PUBLIC void jk_buffer_reverse(JkBuffer buffer);

JK_PUBLIC JkBuffer jk_buffer_alloc(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_buffer_alloc_zero(JkArena *arena, int64_t size);

JK_PUBLIC int64_t jk_strlen(char *string);

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC char *jk_buffer_to_null_terminated(JkArena *arena, JkBuffer buffer);

JK_PUBLIC int32_t jk_buffer_character_get(JkBuffer buffer, int64_t pos);

JK_PUBLIC int32_t jk_buffer_character_next(JkBuffer buffer, int64_t *pos);

JK_PUBLIC uint32_t jk_buffer_bits_peek(JkBuffer buffer, int64_t bit_cursor, int64_t bit_count);

JK_PUBLIC uint32_t jk_buffer_bits_read(JkBuffer buffer, int64_t *bit_cursor, int64_t bit_count);

JK_PUBLIC JkBuffer jk_buffer_null_terminated_next(JkBuffer buffer, int64_t *pos);

#define JK_BUFFER_FIELD_NEXT(buffer, pos, type) \
    (*(pos) += JK_SIZEOF(type),                 \
            (*(pos) < (buffer).size ? (type *)((buffer).data + (*(pos) - JK_SIZEOF(type))) : 0))

#define JK_BUFFER_FIELD_READ(buffer, pos, type, default)                                    \
    (*(pos) += JK_SIZEOF(type),                                                             \
            (*(pos) < (buffer).size ? *(type *)((buffer).data + (*(pos) - JK_SIZEOF(type))) \
                                    : (default)))

JK_PUBLIC int32_t jk_buffer_compare(JkBuffer a, JkBuffer b);

JK_PUBLIC uint64_t jk_buffer_hash(JkBuffer buffer);

JK_PUBLIC b32 jk_char_is_whitespace(int32_t c);

JK_PUBLIC b32 jk_char_is_digit(int32_t c);

JK_PUBLIC int32_t jk_char_to_lower(int32_t c);

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string);

JK_PUBLIC int64_t jk_string_find(JkBuffer string, JkBuffer substring);

JK_PUBLIC JkBuffer jk_int_to_string(JkArena *arena, int64_t value);

JK_PUBLIC JkBuffer jk_unsigned_to_string(JkArena *arena, uint64_t value, int64_t min_width);

JK_PUBLIC uint8_t jk_hex_char[16];

JK_PUBLIC JkBuffer jk_unsigned_to_hexadecimal_string(
        JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkBuffer jk_unsigned_to_binary_string(JkArena *arena, uint64_t value, int16_t min_width);

JK_PUBLIC JkBuffer jk_f64_to_string(JkArena *arena, double value, int64_t decimal_places);

typedef enum JkFormatItemType {
    JK_FORMAT_ITEM_NULL_TERMINATED,
    JK_FORMAT_ITEM_STRING,
    JK_FORMAT_ITEM_INT,
    JK_FORMAT_ITEM_UNSIGNED,
    JK_FORMAT_ITEM_HEX,
    JK_FORMAT_ITEM_BINARY,
    JK_FORMAT_ITEM_FLOAT,
    JK_FORMAT_ITEM_VEC3,
    JK_FORMAT_ITEM_VEC4,
    JK_FORMAT_ITEM_BYTES,

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
        JkVec3 vec3_value;
        JkVec4 vec4_value;
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

JK_PUBLIC JkFormatItem jkfv(JkVec3 v);

JK_PUBLIC JkFormatItem jkfv4(JkVec4 v);

JK_PUBLIC JkFormatItem jkf_bytes(double byte_count);

JK_PUBLIC JkFormatItem jkf_nl;

JK_PUBLIC JkBuffer jk_format(JkArena *arena, JkFormatItemArray items);

#define JK_FORMAT(arena, ...)                                                                  \
    jk_format(arena,                                                                           \
            (JkFormatItemArray){                                                               \
                .count = JK_SIZEOF(((JkFormatItem[]){__VA_ARGS__})) / JK_SIZEOF(JkFormatItem), \
                .items = (JkFormatItem[]){__VA_ARGS__}})

JK_PUBLIC void (*jk_print)(JkBuffer string);

JK_PUBLIC void jk_print_fmt(JkArena *arena, JkFormatItemArray items);

#define JK_PRINT_FMT(arena, ...)                                                               \
    jk_print_fmt(arena,                                                                        \
            (JkFormatItemArray){                                                               \
                .count = JK_SIZEOF(((JkFormatItem[]){__VA_ARGS__})) / JK_SIZEOF(JkFormatItem), \
                .items = (JkFormatItem[]){__VA_ARGS__}})

JK_PUBLIC JkBuffer jk_path_directory(JkBuffer path);

JK_PUBLIC JkBuffer jk_path_basename(JkBuffer path);

// ---- Buffer end -------------------------------------------------------------

// ---- Logging begin ----------------------------------------------------------

typedef enum JkLogType {
    JK_LOG_NIL,
    JK_LOG_INFO,
    JK_LOG_WARNING,
    JK_LOG_ERROR,
    JK_LOG_FATAL,
    JK_LOG_TYPE_COUNT,
} JkLogType;

typedef struct JkLogEntry {
    int64_t i;
} JkLogEntry;

typedef struct JkLogEntryData {
    JkLogEntry next;
    JkLogEntry prev;

    JkLogType type;
    JkSpan message;
} JkLogEntryData;

typedef struct JkLog {
    void (*print)(JkBuffer string);

    int64_t entry_slot_next;
    int64_t entry_slot_max;
    int64_t string_buffer_start;
    int64_t string_buffer_capacity;
    int64_t string_offset_next;

    uint8_t scratch_buffer[4096];

    // Array of size max_entry_count. entries[0] is the log sentinel node. entries[1] is the free
    // list sentinel node.
    JkLogEntryData entries[2];
} JkLog;

JK_PUBLIC JkLog *jk_log_init(void (*print)(JkBuffer message), JkBuffer memory);

JK_PUBLIC b32 jk_log_valid(JkLog *l);

JK_PUBLIC void jk_log(JkLog *l, JkLogType type, JkBuffer message);

JK_PUBLIC b32 jk_log_entry_equal(JkLogEntry a, JkLogEntry b);

JK_PUBLIC b32 jk_log_entry_valid(JkLogEntry entry);

JK_PUBLIC JkLogType jk_log_entry_type(JkLog *l, JkLogEntry entry);

JK_PUBLIC JkBuffer jk_log_entry_message(JkLog *l, JkLogEntry entry);

JK_PUBLIC JkLogEntry jk_log_entry_first(JkLog *l);

JK_PUBLIC JkLogEntry jk_log_entry_next(JkLog *l, JkLogEntry entry);

JK_PUBLIC void jk_log_entry_print(JkLog *l, JkLogEntry entry);

JK_PUBLIC void jk_log_entry_remove(JkLog *l, JkLogEntry entry);

#define JK_LOG_ITER(log, entry_field_name)                                                         \
    for (JkLogEntry next, entry_field_name = jk_log_entry_first(log);                              \
            next = jk_log_entry_next(log, entry_field_name), jk_log_entry_valid(entry_field_name); \
            entry_field_name = next)

// ---- Logging end ------------------------------------------------------------

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

JK_PUBLIC float jk_tan_f32(float value);

JK_PUBLIC float jk_acos_core_f32(float value);

JK_PUBLIC float jk_acos_f32(float value);

// ---- Math end ---------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

JK_PUBLIC JkArena jk_arena_fixed_init(JkArenaRoot *root, JkBuffer memory);

JK_PUBLIC b32 jk_arena_valid(JkArena *arena);

JK_PUBLIC void *jk_arena_push(JkArena *arena, int64_t size);

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_arena_push_buffer(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_arena_push_buffer_zero(JkArena *arena, int64_t size);

JK_PUBLIC JkBuffer jk_arena_as_buffer(JkArena *arena);

#define JK_ARENA_PUSH_ARRAY(arena, array, item_count)                                   \
    do {                                                                                \
        (array).count = item_count;                                                     \
        (array).items = jk_arena_push(arena, (item_count) * JK_SIZEOF(*(array).items)); \
    } while (0)

JK_PUBLIC void jk_arena_pop(JkArena *arena, int64_t size);

JK_PUBLIC JkArena jk_arena_child_get(JkArena *parent);

JK_PUBLIC void jk_arena_child_commit(JkArena *parent, JkArena *child);

JK_PUBLIC void *jk_arena_pointer_current(JkArena *arena);

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
        JkBuffer buffer, int64_t *pos, JkUtf8Codepoint *codepoint);

// ---- UTF-8 end --------------------------------------------------------------

// ---- Quicksort begin --------------------------------------------------------

JK_PUBLIC void jk_quicksort(void *array_void,
        int64_t element_count,
        int64_t element_size,
        void *tmp,
        void *data,
        int (*compare)(void *data, void *a, void *b));

JK_PUBLIC void jk_quicksort_ints(int32_t *array, int32_t length);

JK_PUBLIC void jk_quicksort_floats(float *array, int32_t length);

JK_PUBLIC void jk_quicksort_strings(char **array, int32_t length);

// ---- Quicksort end ----------------------------------------------------------

// ---- JkIntVec2 begin --------------------------------------------------------

typedef union JkIntVec2 {
    int32_t v[2];
    struct {
        int32_t x;
        int32_t y;
    };
} JkIntVec2;

typedef struct JkIntVec2Array {
    int64_t count;
    JkIntVec2 *items;
} JkIntVec2Array;

JK_PUBLIC b32 jk_int_vec2_equal(JkIntVec2 a, JkIntVec2 b);

JK_PUBLIC JkIntVec2 jk_int_vec2_add(JkIntVec2 a, JkIntVec2 b);

JK_PUBLIC JkIntVec2 jk_int_vec2_sub(JkIntVec2 a, JkIntVec2 b);

JK_PUBLIC JkIntVec2 jk_int_vec2_mul(int32_t scalar, JkIntVec2 vector);

JK_PUBLIC JkIntVec2 jk_int_vec2_div(int32_t divisor, JkIntVec2 vector);

JK_PUBLIC JkIntVec2 jk_int_vec2_remainder(int32_t divisor, JkIntVec2 vector);

// ---- JkIntVec2 end ----------------------------------------------------------

// ---- JkVec2 begin -----------------------------------------------------------

typedef union JkVec2 {
    float v[2];
    struct {
        float x;
        float y;
    };
} JkVec2;

typedef struct JkVec2Array {
    int64_t count;
    JkVec2 *items;
} JkVec2Array;

JK_PUBLIC b32 jk_vec2_approx_equal(JkVec2 a, JkVec2 b, float tolerance);

JK_PUBLIC JkVec2 jk_vec2_add(JkVec2 a, JkVec2 b);

JK_PUBLIC JkVec2 jk_vec2_sub(JkVec2 a, JkVec2 b);

JK_PUBLIC JkVec2 jk_vec2_mul(float scalar, JkVec2 vector);

JK_PUBLIC JkVec2 jk_vec2_ceil(JkVec2 v);

JK_PUBLIC JkIntVec2 jk_vec2_ceil_i(JkVec2 v);

JK_PUBLIC float jk_vec2_magnitude_sqr(JkVec2 v);

JK_PUBLIC float jk_vec2_magnitude(JkVec2 v);

JK_PUBLIC JkVec2 jk_vec2_normalized(JkVec2 v);

JK_PUBLIC float jk_vec2_dot(JkVec2 u, JkVec2 v);

JK_PUBLIC float jk_vec2_cross(JkVec2 u, JkVec2 v);

JK_PUBLIC float jk_vec2_angle_between(JkVec2 u, JkVec2 v);

JK_PUBLIC JkVec2 jk_vec2_lerp(JkVec2 a, JkVec2 b, float t);

JK_PUBLIC float jk_vec2_distance_squared(JkVec2 a, JkVec2 b);

JK_PUBLIC JkVec2 jk_vec2_from_int(JkIntVec2 int_vector);

JK_PUBLIC JkIntVec2 jk_vec2_round(JkVec2 vector);

JK_PUBLIC JkVec2 jk_matrix_2x2_multiply_vector(float matrix[2][2], JkVec2 vector);

// ---- JkVec2 end -------------------------------------------------------------

// ---- JkVec3 begin -----------------------------------------------------------

typedef struct JkVec3Array {
    int64_t count;
    JkVec3 *items;
} JkVec3Array;

JK_PUBLIC b32 jk_vec3_equal(JkVec3 a, JkVec3 b, float tolerance);

JK_PUBLIC JkVec3 jk_vec3_add(JkVec3 a, JkVec3 b);

JK_PUBLIC JkVec3 jk_vec3_sub(JkVec3 a, JkVec3 b);

JK_PUBLIC JkVec3 jk_vec3_mul(float scalar, JkVec3 vector);

JK_PUBLIC JkVec3 jk_vec3_hadamard_prod(JkVec3 u, JkVec3 v);

JK_PUBLIC JkVec3 jk_vec3_ceil(JkVec3 v);

JK_PUBLIC float jk_vec3_magnitude_sqr(JkVec3 v);

JK_PUBLIC float jk_vec3_magnitude(JkVec3 v);

JK_PUBLIC JkVec3 jk_vec3_normalized(JkVec3 v);

JK_PUBLIC float jk_vec3_dot(JkVec3 u, JkVec3 v);

JK_PUBLIC JkVec3 jk_vec3_cross(JkVec3 u, JkVec3 v);

JK_PUBLIC float jk_vec3_angle_between(JkVec3 u, JkVec3 v);

JK_PUBLIC JkVec3 jk_vec3_lerp(JkVec3 a, JkVec3 b, float t);

JK_PUBLIC float jk_vec3_distance_squared(JkVec3 a, JkVec3 b);

JK_PUBLIC JkVec3 jk_vec3_round(JkVec3 vector);

JK_PUBLIC JkVec2 jk_vec3_to_2(JkVec3 v);

// ---- JkVec3 end -------------------------------------------------------------

// ---- JkVec4 begin -----------------------------------------------------------

typedef struct JkVec4Array {
    int64_t count;
    JkVec4 *items;
} JkVec4Array;

JK_PUBLIC JkVec4 jk_vec4_add(JkVec4 a, JkVec4 b);

JK_PUBLIC JkVec4 jk_vec4_mul(float scalar, JkVec4 v);

JK_PUBLIC float jk_vec4_magnitude_sqr(JkVec4 v);

JK_PUBLIC float jk_vec4_magnitude(JkVec4 v);

JK_PUBLIC JkVec4 jk_vec4_normalized(JkVec4 v);

JK_PUBLIC JkVec4 jk_vec4_lerp(JkVec4 a, JkVec4 b, float t);

JK_PUBLIC JkVec4 jk_vec3_to_4(JkVec3 v, float w);

JK_PUBLIC JkVec2 jk_vec4_to_2(JkVec4 v);

JK_PUBLIC JkVec3 jk_vec4_to_3(JkVec4 v);

JK_PUBLIC JkVec3 jk_vec4_perspective_divide(JkVec4 v);

// ---- JkVec4 end -------------------------------------------------------------

// ---- JkMat4 begin -----------------------------------------------------------

typedef struct JkMat4 {
    float e[4][4];
} JkMat4;

JK_PUBLIC JkMat4 jk_mat4_i;

JK_PUBLIC JkMat4 jk_mat4_transpose(JkMat4 m);

JK_PUBLIC JkMat4 jk_mat4_mul(JkMat4 a, JkMat4 b);

JK_PUBLIC JkVec3 jk_mat4_mul_point(JkMat4 m, JkVec3 v);

JK_PUBLIC JkVec3 jk_mat4_mul_normal(JkMat4 m, JkVec3 v);

JK_PUBLIC JkVec4 jk_mat4_mul_vec4(JkMat4 m, JkVec4 v);

JK_PUBLIC JkMat4 jk_mat4_translate(JkVec3 offset);

JK_PUBLIC JkMat4 jk_mat4_rotate_x(float a);

JK_PUBLIC JkMat4 jk_mat4_rotate_y(float a);

JK_PUBLIC JkMat4 jk_mat4_rotate_z(float a);

JK_PUBLIC JkMat4 jk_mat4_scale(JkVec3 scale);

JK_PUBLIC JkMat4 jk_mat4_perspective(JkIntVec2 dimensions, float fov_radians, float near_clip);

// The order here is important. My preferred coordinate system is x = right, y = forward, z = up,
// which is right-handed. These are ordered such that direction / 2 is the axis and direction % 2
// tells us whether or not we're talking about the negative direction. For example,
// JK_DOWN / 2 = 5 / 2 = 2 (the index of our Z axis). JK_DOWN % 2 = 5 % 2 = 1, so we're talking
// about -Z.
typedef enum JkDirection {
    JK_RIGHT,
    JK_LEFT,
    JK_FORWARD,
    JK_BACKWARD,
    JK_UP,
    JK_DOWN,
    JK_DIRECTION_COUNT,
} JkDirection;

typedef struct JkCoordinateSystem {
    JkDirection direction[3];
} JkCoordinateSystem;

JK_PUBLIC JkMat4 jk_mat4_conversion_from(JkCoordinateSystem source);

JK_PUBLIC JkMat4 jk_mat4_conversion_to(JkCoordinateSystem dest);

JK_PUBLIC JkMat4 jk_mat4_conversion_from_to(JkCoordinateSystem source, JkCoordinateSystem dest);

// ---- JkMat4 end -------------------------------------------------------------

// ---- Quaternion begin -------------------------------------------------------

// Reuse the JkVec4 type for quaternions, but the functions are prefixed jk_quat

JkVec4 jk_quat_angle_axis(float angle, JkVec3 axis);

JkVec4 jk_quat_reverse(JkVec4 q);

JkVec4 jk_quat_mul(JkVec4 a, JkVec4 b);

JkVec3 jk_quat_rotate(JkVec4 q, JkVec3 v);

JkMat4 jk_quat_to_mat4(JkVec4 q);

JkVec4 jk_mat4_to_quat(JkMat4 m);

// ---- Quaternion end ---------------------------------------------------------

// ---- JkTransform begin ------------------------------------------------------

typedef struct JkTransform {
    JkVec3 translation;
    JkVec4 rotation;
    JkVec3 scale;
} JkTransform;

JK_PUBLIC JkMat4 jk_transform_to_mat4(JkTransform t);

JK_PUBLIC JkMat4 jk_transform_to_mat4_inv(JkTransform t);

// ---- JkTransform end --------------------------------------------------------

// ---- Shapes begin -----------------------------------------------------------

typedef union JkSegment2d {
    JkVec2 endpoints[2];
    struct {
        JkVec2 p0;
        JkVec2 p1;
    };
} JkSegment2d;

typedef union JkSegment3d {
    JkVec3 endpoints[2];
    struct {
        JkVec3 p0;
        JkVec3 p1;
    };
} JkSegment3d;

JK_PUBLIC float jk_segment_y_intersection(JkSegment2d segment, float y);

JK_PUBLIC float jk_segment_x_intersection(JkSegment2d segment, float x);

typedef struct JkEdge {
    JkSegment2d segment;
    float direction;
} JkEdge;

typedef struct JkEdgeArray {
    int64_t count;
    JkEdge *items;
} JkEdgeArray;

JK_PUBLIC JkEdge jk_points_to_edge(JkVec2 a, JkVec2 b);

typedef struct JkRect {
    JkVec2 min;
    JkVec2 max;
} JkRect;

typedef struct JkIntRect {
    JkIntVec2 min;
    JkIntVec2 max;
} JkIntRect;

JK_PUBLIC JkRect jk_rect(JkVec2 position, JkVec2 dimensions);

JK_PUBLIC JkIntVec2 jk_int_rect_dimensions(JkIntRect rect);

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVec2 point);

JK_PUBLIC JkIntRect jk_int_rect_intersect(JkIntRect a, JkIntRect b);

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

// ---- JkKeyboard begin -------------------------------------------------------

typedef enum JkKey {
    JK_KEY_NONE = 0x00,
    JK_KEY_ERROR_ROLL_OVER = 0x01,
    JK_KEY_ERROR_POST = 0x02,
    JK_KEY_ERROR_UNDEFINED = 0x03,
    JK_KEY_A = 0x04,
    JK_KEY_B = 0x05,
    JK_KEY_C = 0x06,
    JK_KEY_D = 0x07,
    JK_KEY_E = 0x08,
    JK_KEY_F = 0x09,
    JK_KEY_G = 0x0a,
    JK_KEY_H = 0x0b,
    JK_KEY_I = 0x0c,
    JK_KEY_J = 0x0d,
    JK_KEY_K = 0x0e,
    JK_KEY_L = 0x0f,
    JK_KEY_M = 0x10,
    JK_KEY_N = 0x11,
    JK_KEY_O = 0x12,
    JK_KEY_P = 0x13,
    JK_KEY_Q = 0x14,
    JK_KEY_R = 0x15,
    JK_KEY_S = 0x16,
    JK_KEY_T = 0x17,
    JK_KEY_U = 0x18,
    JK_KEY_V = 0x19,
    JK_KEY_W = 0x1a,
    JK_KEY_X = 0x1b,
    JK_KEY_Y = 0x1c,
    JK_KEY_Z = 0x1d,
    JK_KEY_1 = 0x1e,
    JK_KEY_2 = 0x1f,
    JK_KEY_3 = 0x20,
    JK_KEY_4 = 0x21,
    JK_KEY_5 = 0x22,
    JK_KEY_6 = 0x23,
    JK_KEY_7 = 0x24,
    JK_KEY_8 = 0x25,
    JK_KEY_9 = 0x26,
    JK_KEY_0 = 0x27,
    JK_KEY_ENTER = 0x28,
    JK_KEY_ESC = 0x29,
    JK_KEY_BACKSPACE = 0x2a,
    JK_KEY_TAB = 0x2b,
    JK_KEY_SPACE = 0x2c,
    JK_KEY_MINUS = 0x2d,
    JK_KEY_EQUAL = 0x2e,
    JK_KEY_LEFTBRACE = 0x2f,
    JK_KEY_RIGHTBRACE = 0x30,
    JK_KEY_BACKSLASH = 0x31,
    JK_KEY_HASHTILDE = 0x32,
    JK_KEY_SEMICOLON = 0x33,
    JK_KEY_APOSTROPHE = 0x34,
    JK_KEY_GRAVE = 0x35,
    JK_KEY_COMMA = 0x36,
    JK_KEY_DOT = 0x37,
    JK_KEY_SLASH = 0x38,
    JK_KEY_CAPSLOCK = 0x39,
    JK_KEY_F1 = 0x3a,
    JK_KEY_F2 = 0x3b,
    JK_KEY_F3 = 0x3c,
    JK_KEY_F4 = 0x3d,
    JK_KEY_F5 = 0x3e,
    JK_KEY_F6 = 0x3f,
    JK_KEY_F7 = 0x40,
    JK_KEY_F8 = 0x41,
    JK_KEY_F9 = 0x42,
    JK_KEY_F10 = 0x43,
    JK_KEY_F11 = 0x44,
    JK_KEY_F12 = 0x45,
    JK_KEY_SYSRQ = 0x46,
    JK_KEY_SCROLLLOCK = 0x47,
    JK_KEY_PAUSE = 0x48,
    JK_KEY_INSERT = 0x49,
    JK_KEY_HOME = 0x4a,
    JK_KEY_PAGEUP = 0x4b,
    JK_KEY_DELETE = 0x4c,
    JK_KEY_END = 0x4d,
    JK_KEY_PAGEDOWN = 0x4e,
    JK_KEY_RIGHT = 0x4f,
    JK_KEY_LEFT = 0x50,
    JK_KEY_DOWN = 0x51,
    JK_KEY_UP = 0x52,
    JK_KEY_NUMLOCK = 0x53,
    JK_KEY_KPSLASH = 0x54,
    JK_KEY_KPASTERISK = 0x55,
    JK_KEY_KPMINUS = 0x56,
    JK_KEY_KPPLUS = 0x57,
    JK_KEY_KPENTER = 0x58,
    JK_KEY_KP1 = 0x59,
    JK_KEY_KP2 = 0x5a,
    JK_KEY_KP3 = 0x5b,
    JK_KEY_KP4 = 0x5c,
    JK_KEY_KP5 = 0x5d,
    JK_KEY_KP6 = 0x5e,
    JK_KEY_KP7 = 0x5f,
    JK_KEY_KP8 = 0x60,
    JK_KEY_KP9 = 0x61,
    JK_KEY_KP0 = 0x62,
    JK_KEY_KPDOT = 0x63,
    JK_KEY_102ND = 0x64,
    JK_KEY_COMPOSE = 0x65,
    JK_KEY_POWER = 0x66,
    JK_KEY_KPEQUAL = 0x67,
    JK_KEY_F13 = 0x68,
    JK_KEY_F14 = 0x69,
    JK_KEY_F15 = 0x6a,
    JK_KEY_F16 = 0x6b,
    JK_KEY_F17 = 0x6c,
    JK_KEY_F18 = 0x6d,
    JK_KEY_F19 = 0x6e,
    JK_KEY_F20 = 0x6f,
    JK_KEY_F21 = 0x70,
    JK_KEY_F22 = 0x71,
    JK_KEY_F23 = 0x72,
    JK_KEY_F24 = 0x73,
    JK_KEY_OPEN = 0x74,
    JK_KEY_HELP = 0x75,
    JK_KEY_PROPS = 0x76,
    JK_KEY_FRONT = 0x77,
    JK_KEY_STOP = 0x78,
    JK_KEY_AGAIN = 0x79,
    JK_KEY_UNDO = 0x7a,
    JK_KEY_CUT = 0x7b,
    JK_KEY_COPY = 0x7c,
    JK_KEY_PASTE = 0x7d,
    JK_KEY_FIND = 0x7e,
    JK_KEY_MUTE = 0x7f,
    JK_KEY_VOLUMEUP = 0x80,
    JK_KEY_VOLUMEDOWN = 0x81,
    JK_KEY_LOCKING_CAPSLOCK = 0x82,
    JK_KEY_LOCKING_NUMLOCK = 0x83,
    JK_KEY_LOCKING_SCROLLLOCK = 0x84,
    JK_KEY_KPCOMMA = 0x85,
    JK_KEY_AS_400_EQUAL = 0x86,
    JK_KEY_RO = 0x87,
    JK_KEY_KATAKANAHIRAGANA = 0x88,
    JK_KEY_YEN = 0x89,
    JK_KEY_HENKAN = 0x8a,
    JK_KEY_MUHENKAN = 0x8b,
    JK_KEY_KPJPCOMMA = 0x8c,
    JK_KEY_INTERNATIONAL_7 = 0x8d,
    JK_KEY_INTERNATIONAL_8 = 0x8e,
    JK_KEY_INTERNATIONAL_9 = 0x8f,
    JK_KEY_HANGEUL = 0x90,
    JK_KEY_HANJA = 0x91,
    JK_KEY_KATAKANA = 0x92,
    JK_KEY_HIRAGANA = 0x93,
    JK_KEY_ZENKAKUHANKAKU = 0x94,
    JK_KEY_LANG6 = 0x95,
    JK_KEY_LANG7 = 0x96,
    JK_KEY_LANG8 = 0x97,
    JK_KEY_LANG9 = 0x98,
    JK_KEY_ERASE = 0x99,
    JK_KEY_ATTENTION = 0x9a,
    JK_KEY_CANCEL = 0x9b,
    JK_KEY_CLEAR = 0x9c,
    JK_KEY_PRIOR = 0x9d,
    JK_KEY_RETURN = 0x9e,
    JK_KEY_SEPARATOR = 0x9f,
    JK_KEY_OUT = 0xa0,
    JK_KEY_OPER = 0xa1,
    JK_KEY_CLEAR_AGAIN = 0xa2,
    JK_KEY_CRSEL_PROPS = 0xa3,
    JK_KEY_EXSEL = 0xa4,
    JK_KEY_RESERVED_00 = 0xa5,
    JK_KEY_RESERVED_01 = 0xa6,
    JK_KEY_RESERVED_02 = 0xa7,
    JK_KEY_RESERVED_03 = 0xa8,
    JK_KEY_RESERVED_04 = 0xa9,
    JK_KEY_RESERVED_05 = 0xaa,
    JK_KEY_RESERVED_06 = 0xab,
    JK_KEY_RESERVED_07 = 0xac,
    JK_KEY_RESERVED_08 = 0xad,
    JK_KEY_RESERVED_09 = 0xae,
    JK_KEY_RESERVED_10 = 0xaf,
    JK_KEY_00 = 0xb0,
    JK_KEY_000 = 0xb1,
    JK_KEY_THOUSANDS_SEPARATOR = 0xb2,
    JK_KEY_DECIMAL_SEPARATOR = 0xb3,
    JK_KEY_CURRENCY_UNIT = 0xb4,
    JK_KEY_CURRENCY_SUB_UNIT = 0xb5,
    JK_KEY_KPLEFTPAREN = 0xb6,
    JK_KEY_KPRIGHTPAREN = 0xb7,
    JK_KEY_KPLEFTBRACE = 0xb8,
    JK_KEY_KPRIGHTBRACE = 0xb9,
    JK_KEY_KPTAB = 0xba,
    JK_KEY_KPBACKSPACE = 0xbb,
    JK_KEY_KPA = 0xbc,
    JK_KEY_KPB = 0xbd,
    JK_KEY_KPC = 0xbe,
    JK_KEY_KPD = 0xbf,
    JK_KEY_KPE = 0xc0,
    JK_KEY_KPF = 0xc1,
    JK_KEY_KPXOR = 0xc2,
    JK_KEY_KPCARET = 0xc3,
    JK_KEY_KPPERCENT = 0xc4,
    JK_KEY_KPLESSTHAN = 0xc5,
    JK_KEY_KPGREATERTHAN = 0xc6,
    JK_KEY_KPAND = 0xc7,
    JK_KEY_KPLOGICALAND = 0xc8,
    JK_KEY_KPOR = 0xc9,
    JK_KEY_KPLOCALGICALOR = 0xca,
    JK_KEY_KPCOLON = 0xcb,
    JK_KEY_KPNUMBERSIGN = 0xcc,
    JK_KEY_KPSPACE = 0xcd,
    JK_KEY_KPATSIGN = 0xce,
    JK_KEY_KPEXCLAMATION = 0xcf,
    JK_KEY_KPMEMSTORE = 0xd0,
    JK_KEY_KPMEMRECALL = 0xd1,
    JK_KEY_KPMEMCLEAR = 0xd2,
    JK_KEY_KPMEMADD = 0xd3,
    JK_KEY_KPMEMSUBTRACT = 0xd4,
    JK_KEY_KPMEMMULTIPLY = 0xd5,
    JK_KEY_KPMEMDIVIDE = 0xd6,
    JK_KEY_KPPLUSMINUS = 0xd7,
    JK_KEY_KPCLEAR = 0xd8,
    JK_KEY_KPCLEARENTRY = 0xd9,
    JK_KEY_KPBINARY = 0xda,
    JK_KEY_KPOCTAL = 0xdb,
    JK_KEY_KPDECIMAL = 0xdc,
    JK_KEY_KPHEXADECIMAL = 0xdd,
    JK_KEY_RESERVED_11 = 0xde,
    JK_KEY_RESERVED_12 = 0xdf,
    JK_KEY_LEFTCTRL = 0xe0,
    JK_KEY_LEFTSHIFT = 0xe1,
    JK_KEY_LEFTALT = 0xe2,
    JK_KEY_LEFTMETA = 0xe3,
    JK_KEY_RIGHTCTRL = 0xe4,
    JK_KEY_RIGHTSHIFT = 0xe5,
    JK_KEY_RIGHTALT = 0xe6,
    JK_KEY_RIGHTMETA = 0xe7,
    JK_KEY_COUNT,
} JkKey;

typedef struct JkKeyboard {
    uint8_t down[29];
    uint8_t pressed[29];
    uint8_t released[29];
} JkKeyboard;

JK_PUBLIC void jk_keyboard_clear(JkKeyboard *keyboard);

JK_PUBLIC b32 jk_key_down(JkKeyboard *keyboard, JkKey key);

JK_PUBLIC b32 jk_key_pressed(JkKeyboard *keyboard, JkKey key);

JK_PUBLIC b32 jk_key_released(JkKeyboard *keyboard, JkKey key);

// ---- JkKeyboard end ---------------------------------------------------------

// ---- Profile begin ----------------------------------------------------------

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

#ifndef JK_PROFILE_DISABLE
#define JK_PROFILE_DISABLE 0
#endif

typedef enum JkProfileFrameType {
    JK_PROFILE_FRAME_CURRENT,
    JK_PROFILE_FRAME_MIN,
    JK_PROFILE_FRAME_MAX,
    JK_PROFILE_FRAME_TOTAL,
    JK_PROFILE_FRAME_TYPE_COUNT,
} JkProfileFrameType;

#if JK_PROFILE_DISABLE

#define JK_PROFILE_ZONE_BANDWIDTH_BEGIN(...)
#define JK_PROFILE_ZONE_TIME_BEGIN(...)
#define JK_PROFILE_ZONE_END(...)

#else

typedef enum JkProfileMetric {
    JK_PROFILE_METRIC_ELAPSED_EXCLUSIVE,
    JK_PROFILE_METRIC_ELAPSED_INCLUSIVE,
    JK_PROFILE_METRIC_HIT_COUNT,
    JK_PROFILE_METRIC_BYTE_COUNT,
    JK_PROFILE_METRIC_DEPTH,
    JK_PROFILE_METRIC_COUNT,
} JkProfileMetric;

typedef union JkProfileZoneFrame {
    int64_t a[JK_PROFILE_METRIC_COUNT];
    struct {
        int64_t elapsed_exclusive;
        int64_t elapsed_inclusive;
        int64_t hit_count;
        int64_t byte_count;
        int64_t depth;
    };
} JkProfileZoneFrame;

typedef struct JkProfileZone {
    JkBuffer name;
    JkProfileZoneFrame frames[JK_PROFILE_FRAME_TYPE_COUNT];

#if JK_BUILD_MODE != JK_RELEASE
    int64_t active_count;
#endif

    b32 seen;
} JkProfileZone;

typedef struct JkProfileTiming {
    int64_t saved_elapsed_inclusive;
    JkProfileZone *parent;
    uint64_t start;

#if JK_BUILD_MODE != JK_RELEASE
    JkProfileZone *zone;
    b32 ended;
#endif
} JkProfileTiming;

JK_PUBLIC void jk_profile_zone_begin(
        JkProfileTiming *timing, JkProfileZone *zone, JkBuffer name, int64_t byte_count);

JK_PUBLIC void jk_profile_zone_end(JkProfileTiming *timing);

#define JK_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, byte_count) \
    JkProfileTiming jk_profile_timing__##identifier;            \
    do {                                                        \
        static JkProfileZone jk_profile_time_begin_zone;        \
        jk_profile_zone_begin(&jk_profile_timing__##identifier, \
                &jk_profile_time_begin_zone,                    \
                JKS(#identifier),                               \
                byte_count);                                    \
    } while (0)

#define JK_PROFILE_ZONE_TIME_BEGIN(identifier) JK_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, 0)

#define JK_PROFILE_ZONE_END(identifier) jk_profile_zone_end(&jk_profile_timing__##identifier);

#endif

JK_PUBLIC void jk_profile_frame_begin(void);

JK_PUBLIC void jk_profile_frame_end(void);

JK_PUBLIC JkBuffer jk_profile_report(JkArena *arena, int64_t frequency);

// ---- Profile end ------------------------------------------------------------

#define JK_KILOBYTE (1ll << 10)
#define JK_MEGABYTE (1ll << 20)
#define JK_GIGABYTE (1ll << 30)

typedef struct JkFloatArray {
    int64_t count;
    float *items;
} JkFloatArray;

typedef struct JkDoubleArray {
    int64_t count;
    double *items;
} JkDoubleArray;

typedef struct JkInt32Array {
    int64_t count;
    int32_t *items;
} JkInt32Array;

typedef struct JkInt64Array {
    int64_t count;
    int64_t *items;
} JkInt64Array;

typedef union JkConversionUnion {
    uint64_t uint64_v;
    uint32_t uint32_v;
    uint8_t uint8_v[8];
    double f64;
    float f32;
} JkConversionUnion;

JK_PUBLIC JkConversionUnion jk_infinity_f64;
JK_PUBLIC JkConversionUnion jk_infinity_f32;

typedef struct JkColor3 {
    union {
        struct {
#if defined(__wasm__)
            uint8_t r;
            uint8_t g;
            uint8_t b;
#else
            uint8_t b;
            uint8_t g;
            uint8_t r;
#endif
        };
        uint8_t v[3];
    };
} JkColor3;

typedef struct JkColor {
    union {
        struct {
#if defined(__wasm__)
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

JK_PUBLIC JkColor jk_color3_to_4(JkColor3 color, uint8_t alpha);

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

JK_PUBLIC uint64_t jk_hash_uint64(uint64_t x);

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
