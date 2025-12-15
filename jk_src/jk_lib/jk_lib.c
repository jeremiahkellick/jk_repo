#include "jk_lib.h"

// ---- Compiler-specific implementations begin --------------------------------

#if defined(_MSC_VER) && !defined(__clang__)

// copied from intrin.h
#ifndef __INTRIN_H_
uint64_t __lzcnt64(uint64_t);
typedef union __declspec(intrin_type) __declspec(align(16)) __m128 {
    float m128_f32[4];
    unsigned __int64 m128_u64[2];
    __int8 m128_i8[16];
    __int16 m128_i16[8];
    __int32 m128_i32[4];
    __int64 m128_i64[2];
    unsigned __int8 m128_u8[16];
    unsigned __int16 m128_u16[8];
    unsigned __int32 m128_u32[4];
} __m128;
extern __m128 _mm_setzero_ps(void);
extern __m128 _mm_set_ss(float _A);
extern __m128 _mm_sqrt_ss(__m128 _A);
extern float _mm_cvtss_f32(__m128 _A);
#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_RAISE_EXC 0x00
#define _MM_FROUND_FLOOR _MM_FROUND_TO_NEG_INF | _MM_FROUND_RAISE_EXC
#define _MM_FROUND_CEIL _MM_FROUND_TO_POS_INF | _MM_FROUND_RAISE_EXC
extern __m128 _mm_round_ss(__m128, __m128, int);
#endif

JK_PUBLIC int64_t jk_count_leading_zeros(uint64_t value)
{
    return __lzcnt64(value);
}

JK_PUBLIC float jk_round_f32(float value)
{
    return _mm_cvtss_f32(
            _mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_TO_NEAREST_INT));
}

JK_PUBLIC float jk_floor_f32(float value)
{
    return _mm_cvtss_f32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_FLOOR));
}

JK_PUBLIC float jk_ceil_f32(float value)
{
    return _mm_cvtss_f32(_mm_round_ss(_mm_setzero_ps(), _mm_set_ss(value), _MM_FROUND_CEIL));
}

JK_PUBLIC float jk_sqrt_f32(float value)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
}

#elif defined(__GNUC__) || defined(__clang__)

JK_PUBLIC int64_t jk_count_leading_zeros(uint64_t value)
{
    if (value == 0) {
        return 64;
    } else {
        return __builtin_clzll(value);
    }
}

JK_PUBLIC float jk_round_f32(float value)
{
    return __builtin_roundevenf(value);
}

JK_PUBLIC float jk_floor_f32(float value)
{
    return __builtin_floorf(value);
}

JK_PUBLIC float jk_ceil_f32(float value)
{
    return __builtin_ceilf(value);
}

JK_PUBLIC float jk_sqrt_f32(float value)
{
    return __builtin_sqrtf(value);
}

#endif

// ---- Compiler-specific implementations end ----------------------------------

// ---- Buffer begin -----------------------------------------------------------

JK_PUBLIC void jk_buffer_zero(JkBuffer buffer)
{
    jk_memset(buffer.data, 0, buffer.size);
}

JK_PUBLIC JkBuffer jk_buffer_copy(JkArena *arena, JkBuffer buffer)
{
    JkBuffer result = {.size = buffer.size, .data = jk_arena_push(arena, buffer.size)};
    jk_memcpy(result.data, buffer.data, buffer.size);
    return result;
}

JK_PUBLIC void jk_buffer_reverse(JkBuffer buffer)
{
    for (int64_t i = 0; i < buffer.size / 2; i++) {
        JK_SWAP(buffer.data[i], buffer.data[buffer.size - 1 - i], uint8_t);
    }
}

JK_PUBLIC JkBuffer jk_buffer_alloc(JkArena *arena, int64_t size)
{
    return (JkBuffer){.size = size, .data = jk_arena_push(arena, size)};
}

JK_PUBLIC JkBuffer jk_buffer_alloc_zero(JkArena *arena, int64_t size)
{
    return (JkBuffer){.size = size, .data = jk_arena_push_zero(arena, size)};
}

JK_PUBLIC uint32_t jk_buffer_bits_peek(JkBuffer buffer, int64_t bit_cursor, int64_t bit_count)
{
    JK_DEBUG_ASSERT(0 <= bit_count && bit_count <= 32);
    uint64_t result = 0;

    int64_t byte_index = JK_FLOOR_DIV(bit_cursor, 8);
    int64_t bit_index = JK_MOD(bit_cursor, 8);

    for (int64_t i = 0; i < 8; i++) {
        result >>= 8;
        if (0 <= byte_index && byte_index < buffer.size) {
            result |= (uint64_t)buffer.data[byte_index] << 56;
        }
        byte_index++;
    }

    return (uint32_t)((result >> bit_index) & ((1llu << bit_count) - 1));
}

JK_PUBLIC uint32_t jk_buffer_bits_read(JkBuffer buffer, int64_t *bit_cursor, int64_t bit_count)
{
    uint32_t result = jk_buffer_bits_peek(buffer, *bit_cursor, bit_count);
    *bit_cursor += bit_count;
    return result;
}

JK_PUBLIC int64_t jk_strlen(char *string)
{
    char *pointer = string;
    while (*pointer) {
        pointer++;
    }
    return pointer - string;
}

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string)
{
    if (string) {
        return (JkBuffer){.size = jk_strlen(string), .data = (uint8_t *)string};
    } else {
        return (JkBuffer){0};
    }
}

JK_PUBLIC char *jk_buffer_to_null_terminated(JkArena *arena, JkBuffer buffer)
{
    char *result = jk_arena_push(arena, buffer.size + 1);
    result[buffer.size] = '\0';
    jk_memcpy(result, buffer.data, buffer.size);
    return result;
}

JK_PUBLIC int32_t jk_buffer_character_get(JkBuffer buffer, int64_t pos)
{
    return (0 <= pos && pos < buffer.size) ? buffer.data[pos] : JK_OOB;
}

JK_PUBLIC int32_t jk_buffer_character_next(JkBuffer buffer, int64_t *pos)
{
    int32_t c = jk_buffer_character_get(buffer, *pos);
    (*pos)++;
    return c;
}

JK_PUBLIC JkBuffer jk_buffer_null_terminated_next(JkBuffer buffer, int64_t *pos)
{
    JkBuffer result = {0};
    if (0 <= *pos && *pos < buffer.size) {
        result.data = buffer.data + *pos;
        while (*pos < buffer.size && buffer.data[*pos]) {
            (*pos)++;
        }
        result.size = (buffer.data + *pos) - result.data;
        (*pos)++;
    }
    return result;
}

JK_PUBLIC int jk_buffer_compare(JkBuffer a, JkBuffer b)
{
    for (int64_t pos = 0; 1; pos++) {
        int a_char = jk_buffer_character_get(a, pos);
        int b_char = jk_buffer_character_get(b, pos);
        if (a_char < b_char) {
            return -1;
        } else if (a_char > b_char) {
            return 1;
        } else if (a_char == JK_OOB && b_char == JK_OOB) {
            return 0;
        }
    }
}

JK_PUBLIC b32 jk_char_is_whitespace(int c)
{
    return c == ' ' || ('\t' <= c && c <= '\r');
}

JK_PUBLIC b32 jk_char_is_digit(int c)
{
    return '0' <= c && c <= '9';
}

JK_PUBLIC int jk_char_to_lower(int c)
{
    if ('A' <= c && c <= 'Z') {
        return c + ('a' - 'A');
    } else {
        return c;
    }
}

JK_PUBLIC b32 jk_string_contains_whitespace(JkBuffer string)
{
    for (int64_t i = 0; i < string.size; i++) {
        if (jk_char_is_whitespace(string.data[i])) {
            return 1;
        }
    }
    return 0;
}

// Returns the index where the search_string appears in the text if found, or -1 if not found
JK_PUBLIC int64_t jk_string_find(JkBuffer text, JkBuffer search_string)
{
    for (int64_t i = 0; i <= (int64_t)text.size - (int64_t)search_string.size; i++) {
        b32 match = 1;
        for (int64_t j = 0; j < (int64_t)search_string.size; j++) {
            JK_DEBUG_ASSERT(i + j < (int64_t)text.size);
            JK_DEBUG_ASSERT(j < (int64_t)search_string.size);
            if (text.data[i + j] != search_string.data[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            return i;
        }
    }

    return -1;
}

JK_PUBLIC JkBuffer jk_int_to_string(JkArena *arena, int64_t value)
{
    JkBuffer result;
    result.data = jk_arena_pointer_current(arena);

    b32 negative = value < 0;
    value = JK_ABS(value);
    do {
        uint8_t digit = value % 10;
        uint8_t *c = jk_arena_push(arena, 1);
        if (c) {
            *c = '0' + digit;
        } else {
            return (JkBuffer){0};
        }
        value /= 10;
    } while (value);

    if (negative) {
        uint8_t *c = jk_arena_push(arena, 1);
        if (c) {
            *c = '-';
        } else {
            return (JkBuffer){0};
        }
    }

    result.size = (uint8_t *)jk_arena_pointer_current(arena) - result.data;

    jk_buffer_reverse(result);

    return result;
}

JK_PUBLIC JkBuffer jk_unsigned_to_string(JkArena *arena, uint64_t value, int64_t min_width)
{
    JkBuffer result;
    result.data = jk_arena_pointer_current(arena);

    int16_t width_remaining = min_width;
    do {
        uint8_t digit = value % 10;
        uint8_t *c = jk_arena_push(arena, 1);
        if (c) {
            *c = '0' + digit;
        } else {
            return (JkBuffer){0};
        }
        value /= 10;
        width_remaining--;
    } while (0 < width_remaining || value);

    result.size = (uint8_t *)jk_arena_pointer_current(arena) - result.data;

    jk_buffer_reverse(result);

    return result;
}

static uint8_t jk_hex_char[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

JK_PUBLIC JkBuffer jk_unsigned_to_hexadecimal_string(
        JkArena *arena, uint64_t value, int16_t min_width)
{
    JkBuffer result;
    result.data = jk_arena_pointer_current(arena);

    int16_t width_remaining = min_width;
    do {
        uint8_t *c = jk_arena_push(arena, 1);
        if (c) {
            *c = jk_hex_char[value & 0xf];
        } else {
            return (JkBuffer){0};
        }
        value >>= 4;
        width_remaining--;
    } while (0 < width_remaining || value);

    result.size = (uint8_t *)jk_arena_pointer_current(arena) - result.data;

    jk_buffer_reverse(result);

    return result;
}

JK_PUBLIC JkBuffer jk_unsigned_to_binary_string(JkArena *arena, uint64_t value, int16_t min_width)
{
    JkBuffer result;
    result.data = jk_arena_pointer_current(arena);

    int16_t width_remaining = min_width;
    do {
        uint8_t *c = jk_arena_push(arena, 1);
        if (c) {
            *c = '0' + (value & 1);
        } else {
            return (JkBuffer){0};
        }
        value >>= 1;
        width_remaining--;
    } while (0 < width_remaining || value);

    result.size = (uint8_t *)jk_arena_pointer_current(arena) - result.data;

    jk_buffer_reverse(result);

    return result;
}

JK_PUBLIC JkBuffer jk_f64_to_string(JkArena *arena, double value, int64_t decimal_places)
{
    JK_DEBUG_ASSERT(0 <= decimal_places && decimal_places <= 8);
    JkFloatUnpacked unpacked = jk_unpack_f64(value);
    if (unpacked.exponent == JK_FLOAT_EXPONENT_SPECIAL) {
        if (unpacked.significand) {
            return unpacked.sign ? JKS("-nan") : JKS("nan");
        } else {
            return unpacked.sign ? JKS("-inf") : JKS("inf");
        }
    } else {
        int32_t int_shift = JK_MAX(-64, unpacked.exponent);
        if ((int32_t)jk_count_leading_zeros(unpacked.significand) < int_shift) {
            return unpacked.sign ? JKS("-unprintable") : JKS("unprintable");
        } else {
            uint64_t integer = jk_signed_shift(unpacked.significand, int_shift);
            JkBuffer sign = unpacked.sign ? JKS("-") : JKS("");
            int32_t frac_shift = JK_CLAMP(unpacked.exponent + 34, -64, 64);
            uint64_t binary_fraction =
                    jk_signed_shift(unpacked.significand, frac_shift) & 0x3ffffffffllu;
            uint64_t decimal_fraction =
                    (binary_fraction * 1000000000llu + 0x200000000llu) / 0x400000000llu;

            if (decimal_places) {
                uint64_t divisor = 10;
                for (int64_t i = 0; i < 8 - decimal_places; i++) {
                    divisor *= 10;
                }
                uint64_t one = 10;
                for (int64_t i = 0; i < decimal_places - 1; i++) {
                    one *= 10;
                }

                uint64_t rounded_fraction = (decimal_fraction + divisor / 2) / divisor;
                if (one <= rounded_fraction) {
                    rounded_fraction -= one;
                    integer += 1;
                }

                return JK_FORMAT(arena,
                        jkfs(sign),
                        jkfu(integer),
                        jkfn("."),
                        jkfuw(rounded_fraction, decimal_places));
            } else {
                if (500000000llu <= decimal_fraction) {
                    integer++;
                }
                return JK_FORMAT(arena, jkfs(sign), jkfu(integer));
            }
        }
    }
}

// Include a null terminated string in JK_FORMAT
JK_PUBLIC JkFormatItem jkfn(char *null_terminated)
{
    return (JkFormatItem){
        .type = JK_FORMAT_ITEM_NULL_TERMINATED,
        .null_terminated = null_terminated,
    };
}

// Include a JkBuffer in JK_FORMAT
JK_PUBLIC JkFormatItem jkfs(JkBuffer string)
{
    return (JkFormatItem){.type = JK_FORMAT_ITEM_STRING, .string = string};
}

// Include a signed integer JK_FORMAT
JK_PUBLIC JkFormatItem jkfi(int64_t signed_value)
{
    return (JkFormatItem){.type = JK_FORMAT_ITEM_INT, .signed_value = signed_value};
}

// Include an unsigned integer in JK_FORMAT
JK_PUBLIC JkFormatItem jkfu(uint64_t unsigned_value)
{
    return (JkFormatItem){.type = JK_FORMAT_ITEM_UNSIGNED, .unsigned_value = unsigned_value};
}

// Include an unsigned integer in JK_FORMAT and supply a width
JK_PUBLIC JkFormatItem jkfuw(uint64_t unsigned_value, int16_t min_width)
{
    return (JkFormatItem){
        .type = JK_FORMAT_ITEM_UNSIGNED, .unsigned_value = unsigned_value, .param = min_width};
}

// Include a hexadecimal value in JK_FORMAT
JK_PUBLIC JkFormatItem jkfh(uint64_t hex_value, int16_t min_width)
{
    return (JkFormatItem){
        .type = JK_FORMAT_ITEM_HEX, .unsigned_value = hex_value, .param = min_width};
}

// Include a binary value in JK_FORMAT
JK_PUBLIC JkFormatItem jkfb(uint64_t binary_value, int16_t min_width)
{
    return (JkFormatItem){
        .type = JK_FORMAT_ITEM_BINARY, .unsigned_value = binary_value, .param = min_width};
}

// Include a floating point value in JK_FORMAT
JK_PUBLIC JkFormatItem jkff(double float_value, int16_t decimal_places)
{
    return (JkFormatItem){
        .type = JK_FORMAT_ITEM_FLOAT, .float_value = float_value, .param = decimal_places};
}

// JK_FORMAT argument representing a newline
JK_PUBLIC JkFormatItem jkf_nl = {.type = JK_FORMAT_ITEM_STRING, .string = JKSI("\n")};

JK_PUBLIC JkBuffer jk_format(JkArena *arena, JkFormatItemArray items)
{
    JkBuffer result;
    result.data = jk_arena_pointer_current(arena);

    for (int64_t i = 0; i < items.count; i++) {
        JkFormatItem *item = items.items + i;
        switch (item->type) {
        case JK_FORMAT_ITEM_NULL_TERMINATED: {
            jk_buffer_copy(arena, jk_buffer_from_null_terminated(item->null_terminated));
        } break;

        case JK_FORMAT_ITEM_STRING: {
            jk_buffer_copy(arena, item->string);
        } break;

        case JK_FORMAT_ITEM_INT: {
            jk_int_to_string(arena, item->signed_value);
        } break;

        case JK_FORMAT_ITEM_UNSIGNED: {
            jk_unsigned_to_string(arena, item->unsigned_value, item->param);
        } break;

        case JK_FORMAT_ITEM_HEX: {
            jk_unsigned_to_hexadecimal_string(arena, item->unsigned_value, item->param);
        } break;

        case JK_FORMAT_ITEM_BINARY: {
            jk_unsigned_to_binary_string(arena, item->unsigned_value, item->param);
        } break;

        case JK_FORMAT_ITEM_FLOAT: {
            jk_f64_to_string(arena, item->float_value, item->param);
        } break;

        case JK_FORMAT_ITEM_TYPE_COUNT: {
            JK_ASSERT(0 && "Invalid JkFormatItem type");
        } break;
        }
    }

    result.size = (uint8_t *)jk_arena_pointer_current(arena) - result.data;
    return result;
}

static void jk_print_stub(JkBuffer string) {}

JK_PUBLIC void (*jk_print)(JkBuffer string) = jk_print_stub;

JK_PUBLIC void jk_print_fmt(JkArena *arena, JkFormatItemArray items)
{
    JkArena tmp_arena = jk_arena_child_get(arena);
    JkBuffer string = jk_format(&tmp_arena, items);
    jk_print(string);
}

// ---- Buffer end -------------------------------------------------------------

// ---- Math begin -------------------------------------------------------------

JK_PUBLIC JkFloatUnpacked jk_unpack_f64(double value)
{
    JkFloatUnpacked result;

    uint64_t bits = *(uint64_t *)&value;

    result.sign = (bits >> 63) & 1;
    result.significand = bits & 0xfffffffffffffllu;

    int32_t raw_exponent = (bits >> 52) & 0x7ff;
    if (raw_exponent == 0x7ff) {
        result.exponent = JK_FLOAT_EXPONENT_SPECIAL;
    } else {
        // Subtract the bit-width of the mantissa from the exponent in addition to the usual bias
        // because we want to treat the significand as an unsigned integer instead of fixed-point
        result.exponent = JK_MAX(1, raw_exponent) - 1023 - 52;
        if (raw_exponent) {
            result.significand |= 0x10000000000000llu;
        }
    }

    return result;
}

JK_PUBLIC double jk_pack_f64(JkFloatUnpacked f)
{
    uint64_t result = f.sign ? (1llu << 63) : 0;

    if (f.exponent == JK_FLOAT_EXPONENT_SPECIAL) {
        result |= (0x7ffllu << 52) | (f.significand & 0xfffffffffffffllu);
    } else {
        int8_t target_shift = jk_count_leading_zeros(f.significand) - 12 + 1;
        int32_t new_exponent = JK_MAX(1 - 1023 - 52, f.exponent - target_shift);
        if (1023 - 52 < new_exponent) { // Exceeded max exponent, return infinity
            result |= 0x7f800000;
        } else {
            uint64_t shifted_significand =
                    jk_signed_shift(f.significand, f.exponent - new_exponent);
            result |= shifted_significand & 0xfffffffffffffllu;

            if (shifted_significand & 0x10000000000000llu) {
                result |= (uint64_t)((new_exponent + 1023 + 52) & 0x7ff) << 52;
            }
        }
    }

    return *(double *)&result;
}

JK_PUBLIC float jk_remainder_f32(float x, float y)
{
    return x - y * jk_round_f32(x / y);
}

JK_PUBLIC float jk_sin_core_f32(float x)
{
    float result = -0x1.f647bep-11f;
    result = result * x + 0x1.501da2p-7f;
    result = result * x + -0x1.017a88p-9f;
    result = result * x + -0x1.5332bcp-3f;
    result = result * x + -0x1.1624cep-12f;
    result = result * x + 0x1.0001aap+0f;
    result = result * x + -0x1.aaa5e8p-22f;
    return result;
}

JK_PUBLIC float jk_sin_f32(float x)
{
    x = jk_remainder_f32(x, 2 * JK_PI);

    b32 positive = 0 <= x;
    if (!positive) {
        x = -x;
    }
    if (JK_PI / 2 < x) {
        x = JK_PI - x;
    }

    float result = jk_sin_core_f32(x);

    if (!positive) {
        result = -result;
    }

    return result;
}

JK_PUBLIC float jk_cos_f32(float x)
{
    return jk_sin_f32(x + JK_PI / 2);
}

JK_PUBLIC float jk_tan_f32(float value)
{
    return jk_sin_f32(value) / jk_cos_f32(value);
}

JK_PUBLIC float jk_acos_core_f32(float x)
{
    float result = -0x1.056a66p+0f;
    result = result * x + 0x1.1fa76cp+1f;
    result = result * x + -0x1.1b0890p+1f;
    result = result * x + 0x1.10ded6p+0f;
    result = result * x + -0x1.53d396p-2f;
    result = result * x + -0x1.d112acp-4f;
    result = result * x + -0x1.14d2a8p-8f;
    result = result * x + -0x1.ffef0cp-1f;
    result = result * x + 0x1.921faap+0f;
    return result;
}

JK_PUBLIC float jk_acos_f32(float x)
{
    b32 positive = 0.0f <= x;
    if (!positive) {
        x = -x;
    }
    b32 in_standard_range = x <= (float)JK_INV_SQRT_2;
    if (!in_standard_range) {
        x = jk_sqrt_f32(1.0f - x * x);
    }

    float result = jk_acos_core_f32(x);

    if (!in_standard_range) {
        result = (float)(JK_PI / 2.0) - result;
    }
    if (!positive) {
        result = (float)JK_PI - result;
    }

    return result;
}

// ---- Math end ---------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

static b32 jk_arena_fixed_grow(JkArena *arena, int64_t new_size)
{
    return 0; // Fixed arenas don't grow, duh
}

JK_PUBLIC JkArena jk_arena_fixed_init(JkArenaRoot *root, JkBuffer memory)
{
    root->memory = memory;
    return (JkArena){.root = root, .grow = jk_arena_fixed_grow};
}

JK_PUBLIC b32 jk_arena_valid(JkArena *arena)
{
    return !!arena->root;
}

JK_PUBLIC void *jk_arena_push(JkArena *arena, int64_t size)
{
    JK_DEBUG_ASSERT(0 <= size);
    int64_t new_pos = arena->pos + size;
    if (arena->root->memory.size < new_pos) {
        if (!arena->grow(arena, new_pos)) {
            return 0;
        }
    }
    void *address = arena->root->memory.data + arena->pos;
    arena->pos = new_pos;
    return address;
}

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, int64_t size)
{
    void *address = jk_arena_push(arena, size);
    jk_memset(address, 0, size);
    return address;
}

JK_PUBLIC JkBuffer jk_arena_push_buffer(JkArena *arena, int64_t size)
{
    return (JkBuffer){.size = size, .data = jk_arena_push(arena, size)};
}

JK_PUBLIC JkBuffer jk_arena_push_buffer_zero(JkArena *arena, int64_t size)
{
    return (JkBuffer){.size = size, .data = jk_arena_push_zero(arena, size)};
}

JK_PUBLIC void jk_arena_pop(JkArena *arena, int64_t size)
{
    JK_DEBUG_ASSERT(0 <= size && size <= arena->pos - arena->base);
    arena->pos -= size;
}

JK_PUBLIC JkArena jk_arena_child_get(JkArena *parent)
{
    return (JkArena){
        .base = parent->pos,
        .pos = parent->pos,
        .root = parent->root,
        .grow = parent->grow,
    };
}

JK_PUBLIC void *jk_arena_pointer_current(JkArena *arena)
{
    return arena->root->memory.data + arena->pos;
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
        JkBuffer buffer, int64_t *pos, JkUtf8Codepoint *codepoint)
{
    *codepoint = (JkUtf8Codepoint){0};
    if (!(0 <= *pos && *pos < buffer.size)) {
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

// ---- Quicksort begin --------------------------------------------------------

static void jk_bytes_swap(void *a, void *b, int64_t element_size, void *tmp)
{
    jk_memcpy(tmp, a, element_size);
    jk_memcpy(a, b, element_size);
    jk_memcpy(b, tmp, element_size);
}

static void jk_quicksort_internal(JkRandomGeneratorU64 *generator,
        void *array_void,
        int64_t element_count,
        int64_t element_size,
        void *tmp,
        void *data,
        int (*compare)(void *data, void *a, void *b))
{
    JK_DEBUG_ASSERT(0 <= element_size);

    if (element_count < 2) {
        return;
    }

    char *array = array_void;
    char *low = array;
    char *mid = array + element_size;
    char *high = array + (element_count - 1) * element_size;

    // Move random element to start to use as pivot
    char *random_element = array + (jk_random_u64(generator) % element_count) * element_size;
    jk_bytes_swap(array, random_element, element_size, tmp);

    while (mid <= high) {
        // Compare mid with pivot. Pivot is always 1 element before mid.
        int comparison = compare(data, mid, mid - element_size);

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

    int64_t left_count = (int64_t)(low - array) / element_size;
    int64_t right_count = element_count - (int64_t)(mid - array) / element_size;
    jk_quicksort_internal(generator, array, left_count, element_size, tmp, data, compare);
    jk_quicksort_internal(generator, mid, right_count, element_size, tmp, data, compare);
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
        int64_t element_count,
        int64_t element_size,
        void *tmp,
        void *data,
        int (*compare)(void *data, void *a, void *b))
{
    JkRandomGeneratorU64 generator = jk_random_generator_new_u64(0x9646e4db8d81f399);
    jk_quicksort_internal(&generator, array_void, element_count, element_size, tmp, data, compare);
}

static int jk_int_compare(void *data, void *a, void *b)
{
    return *(int *)a - *(int *)b;
}

JK_PUBLIC void jk_quicksort_ints(int *array, int length)
{
    int tmp;
    jk_quicksort(array, length, JK_SIZEOF(int), &tmp, 0, jk_int_compare);
}

static int jk_float_compare(void *data, void *a, void *b)
{
    float delta = *(float *)a - *(float *)b;
    return delta == 0.0f ? 0 : (delta < 0.0f ? -1 : 1);
}

JK_PUBLIC void jk_quicksort_floats(float *array, int length)
{
    float tmp;
    jk_quicksort(array, length, JK_SIZEOF(float), &tmp, 0, jk_float_compare);
}

static int jk_string_compare(void *data, void *a, void *b)
{
    uint8_t *a_ptr = *(uint8_t **)a;
    uint8_t *b_ptr = *(uint8_t **)b;
    for (; *a_ptr || *b_ptr; a_ptr++, b_ptr++) {
        if (*a_ptr < *b_ptr) {
            return -1;
        }
        if (*b_ptr < *a_ptr) {
            return 1;
        }
    }
    return 0;
}

JK_PUBLIC void jk_quicksort_strings(char **array, int length)
{
    char *tmp;
    jk_quicksort(array, length, JK_SIZEOF(char *), &tmp, 0, jk_string_compare);
}

// ---- Quicksort end ----------------------------------------------------------

// ---- JkIntVec2 begin --------------------------------------------------------

JK_PUBLIC b32 jk_int_vec2_equal(JkIntVec2 a, JkIntVec2 b)
{
    return a.x == b.x && a.y == b.y;
}

JK_PUBLIC JkIntVec2 jk_int_vec2_add(JkIntVec2 a, JkIntVec2 b)
{
    return (JkIntVec2){.x = a.x + b.x, .y = a.y + b.y};
}

JK_PUBLIC JkIntVec2 jk_int_vec2_sub(JkIntVec2 a, JkIntVec2 b)
{
    return (JkIntVec2){.x = a.x - b.x, .y = a.y - b.y};
}

JK_PUBLIC JkIntVec2 jk_int_vec2_mul(int32_t scalar, JkIntVec2 vector)
{
    return (JkIntVec2){.x = scalar * vector.x, .y = scalar * vector.y};
}

JK_PUBLIC JkIntVec2 jk_int_vec2_div(int32_t divisor, JkIntVec2 vector)
{
    return (JkIntVec2){.x = vector.x / divisor, .y = vector.y / divisor};
}

JK_PUBLIC JkIntVec2 jk_int_vec2_remainder(int32_t divisor, JkIntVec2 vector)
{
    return (JkIntVec2){.x = vector.x % divisor, .y = vector.y % divisor};
}

// ---- JkIntVec2 end ----------------------------------------------------------

// ---- JkVec2 begin -----------------------------------------------------------

JK_PUBLIC b32 jk_vec2_approx_equal(JkVec2 a, JkVec2 b, float tolerance)
{
    return jk_float32_equal(a.x, b.x, tolerance) && jk_float32_equal(a.y, b.y, tolerance);
}

JK_PUBLIC JkVec2 jk_vec2_add(JkVec2 a, JkVec2 b)
{
    return (JkVec2){.x = a.x + b.x, .y = a.y + b.y};
}

JK_PUBLIC JkVec2 jk_vec2_sub(JkVec2 a, JkVec2 b)
{
    return (JkVec2){.x = a.x - b.x, .y = a.y - b.y};
}

JK_PUBLIC JkVec2 jk_vec2_mul(float scalar, JkVec2 vector)
{
    return (JkVec2){.x = scalar * vector.x, .y = scalar * vector.y};
}

JK_PUBLIC JkVec2 jk_vec2_ceil(JkVec2 v)
{
    return (JkVec2){jk_ceil_f32(v.x), jk_ceil_f32(v.y)};
}

JK_PUBLIC JkIntVec2 jk_vec2_ceil_i(JkVec2 v)
{
    JkVec2 v_ceiled = jk_vec2_ceil(v);
    return (JkIntVec2){(int32_t)v_ceiled.x, (int32_t)v_ceiled.y};
}

JK_PUBLIC float jk_vec2_magnitude_sqr(JkVec2 v)
{
    return v.x * v.x + v.y * v.y;
}

JK_PUBLIC float jk_vec2_magnitude(JkVec2 v)
{
    return jk_sqrt_f32(jk_vec2_magnitude_sqr(v));
}

JK_PUBLIC JkVec2 jk_vec2_normalized(JkVec2 v)
{
    return jk_vec2_mul(1.0f / jk_vec2_magnitude(v), v);
}

JK_PUBLIC float jk_vec2_dot(JkVec2 u, JkVec2 v)
{
    return u.x * v.x + u.y * v.y;
}

JK_PUBLIC float jk_vec2_angle_between(JkVec2 u, JkVec2 v)
{
    float sign = u.x * v.y - u.y * v.x < 0.0f ? -1.0f : 1.0f;
    return sign * jk_acos_f32(jk_vec2_dot(u, v) / (jk_vec2_magnitude(u) * jk_vec2_magnitude(v)));
}

JK_PUBLIC JkVec2 jk_vec2_lerp(JkVec2 a, JkVec2 b, float t)
{
    return jk_vec2_add(jk_vec2_mul(1.0f - t, a), jk_vec2_mul(t, b));
}

JK_PUBLIC float jk_vec2_distance_squared(JkVec2 a, JkVec2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return dx * dx + dy * dy;
}

JK_PUBLIC JkVec2 jk_vec2_from_int(JkIntVec2 int_vector)
{
    return (JkVec2){(float)int_vector.x, (float)int_vector.y};
}

JK_PUBLIC JkIntVec2 jk_vec2_round(JkVec2 vector)
{
    return (JkIntVec2){jk_round(vector.x), jk_round(vector.y)};
}

JK_PUBLIC JkVec2 jk_matrix_2x2_multiply_vector(float matrix[2][2], JkVec2 vector)
{
    return (JkVec2){
        matrix[0][0] * vector.x + matrix[0][1] * vector.y,
        matrix[1][0] * vector.x + matrix[1][1] * vector.y,
    };
}

// ---- JkVec2 end -------------------------------------------------------------

// ---- JkVec3 begin -----------------------------------------------------------

JK_PUBLIC b32 jk_vec3_approx_equal(JkVec3 a, JkVec3 b, float tolerance)
{
    return jk_float32_equal(a.x, b.x, tolerance) && jk_float32_equal(a.y, b.y, tolerance)
            && jk_float32_equal(a.z, b.z, tolerance);
}

JK_PUBLIC JkVec3 jk_vec3_add(JkVec3 a, JkVec3 b)
{
    return (JkVec3){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

JK_PUBLIC JkVec3 jk_vec3_sub(JkVec3 a, JkVec3 b)
{
    return (JkVec3){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

JK_PUBLIC JkVec3 jk_vec3_mul(float scalar, JkVec3 vector)
{
    return (JkVec3){.x = scalar * vector.x, .y = scalar * vector.y, .z = scalar * vector.z};
}

JK_PUBLIC JkVec3 jk_vec3_ceil(JkVec3 v)
{
    return (JkVec3){jk_ceil_f32(v.x), jk_ceil_f32(v.y), jk_ceil_f32(v.z)};
}

JK_PUBLIC float jk_vec3_magnitude_sqr(JkVec3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

JK_PUBLIC float jk_vec3_magnitude(JkVec3 v)
{
    return jk_sqrt_f32(jk_vec3_magnitude_sqr(v));
}

JK_PUBLIC JkVec3 jk_vec3_normalized(JkVec3 v)
{
    return jk_vec3_mul(1.0f / jk_vec3_magnitude(v), v);
}

JK_PUBLIC float jk_vec3_dot(JkVec3 u, JkVec3 v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

JK_PUBLIC JkVec3 jk_vec3_cross(JkVec3 u, JkVec3 v)
{
    return (JkVec3){
        .x = u.y * v.z - u.z * v.y,
        .y = u.z * v.x - u.x * v.z,
        .z = u.x * v.y - u.y * v.x,
    };
}

JK_PUBLIC float jk_vec3_angle_between(JkVec3 u, JkVec3 v)
{
    return jk_acos_f32(jk_vec3_dot(u, v) / (jk_vec3_magnitude(u) * jk_vec3_magnitude(v)));
}

JK_PUBLIC JkVec3 jk_vec3_lerp(JkVec3 a, JkVec3 b, float t)
{
    return jk_vec3_add(jk_vec3_mul(1.0f - t, a), jk_vec3_mul(t, b));
}

JK_PUBLIC float jk_vec3_distance_squared(JkVec3 a, JkVec3 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

JK_PUBLIC JkVec3 jk_vec3_round(JkVec3 vector)
{
    return (JkVec3){jk_round_f32(vector.x), jk_round_f32(vector.y)};
}

JK_PUBLIC JkVec2 jk_vec3_to_2(JkVec3 v)
{
    return (JkVec2){v.x, v.y};
}

// ---- JkVec3 end -------------------------------------------------------------

// ---- JkMat4 begin -----------------------------------------------------------

JK_PUBLIC JkMat4 const jk_mat4_i = {{
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1},
}};

JK_PUBLIC JkMat4 jk_mat4_mul(JkMat4 a, JkMat4 b)
{
    JkMat4 result = {0};
    for (int64_t i = 0; i < 4; i++) {
        for (int64_t j = 0; j < 4; j++) {
            for (int64_t k = 0; k < 4; k++) {
                result.e[i][j] += a.e[i][k] * b.e[k][j];
            }
        }
    }
    return result;
}

JK_PUBLIC JkVec3 jk_mat4_mul_vec3(JkMat4 m, JkVec3 v)
{
    JkVec3 result = {0};
    float w = m.e[3][0] * v.coords[0] + m.e[3][1] * v.coords[1] + m.e[3][2] * v.coords[2]
            + m.e[3][3] * 1;
    for (int64_t i = 0; i < 3; i++) {
        for (int64_t k = 0; k < 3; k++) {
            result.coords[i] += m.e[i][k] * v.coords[k];
        }
        result.coords[i] += m.e[i][3] * 1;
        result.coords[i] /= w;
    }
    return result;
}

// clang-format off
JK_PUBLIC JkMat4 jk_mat4_translate(JkVec3 v)
{
    return (JkMat4){{
        {1, 0, 0, v.x},
        {0, 1, 0, v.y},
        {0, 0, 1, v.z},
        {0, 0, 0,   1},
    }};
}

JK_PUBLIC JkMat4 jk_mat4_rotate_x(float a)
{
    return (JkMat4){{
        {1,             0,              0, 0},
        {0, jk_cos_f32(a), -jk_sin_f32(a), 0},
        {0, jk_sin_f32(a),  jk_cos_f32(a), 0},
        {0,             0,              0, 1},
    }};
}

JK_PUBLIC JkMat4 jk_mat4_rotate_y(float a)
{
    return (JkMat4){{
        { jk_cos_f32(a), 0, jk_sin_f32(a), 0},
        {             0, 1,             0, 0},
        {-jk_sin_f32(a), 0, jk_cos_f32(a), 0},
        {             0, 0,             0, 1},
    }};
}

JK_PUBLIC JkMat4 jk_mat4_rotate_z(float a)
{
    return (JkMat4){{
        {jk_cos_f32(a), -jk_sin_f32(a), 0, 0},
        {jk_sin_f32(a),  jk_cos_f32(a), 0, 0},
        {            0,              0, 1, 0},
        {            0,              0, 0, 1},
    }};
}

JK_PUBLIC JkMat4 jk_mat4_scale(JkVec3 v)
{
    return (JkMat4){{
        {v.x,   0,   0, 0},
        {0,   v.y,   0, 0},
        {0,     0, v.z, 0},
        {0,     0,   0, 1},
    }};
}

JK_PUBLIC JkMat4 jk_mat4_perspective(JkIntVec2 dimensions, float fov_radians, float near_clip)
{
    float inv_tan = 1 / jk_tan_f32(fov_radians / 2);
    if (dimensions.x < dimensions.y) {
        float r = (float)dimensions.x / (float)dimensions.y;
        return (JkMat4){{
            {  inv_tan,         0,  0,         0},
            {        0, r*inv_tan,  0,         0},
            {        0,         0,  0, near_clip},
            {        0,         0, -1,         0},
        }};
    } else {
        float r = (float)dimensions.y / (float)dimensions.x;
        return (JkMat4){{
            {r*inv_tan,         0,  0,         0},
            {        0,   inv_tan,  0,         0},
            {        0,         0,  0, near_clip},
            {        0,         0, -1,         0},
        }};
    }
}
// clang-format on

// Conversion matrix from a given source coordinate system to my preferred one,
// x = right, y = forward, z = up (which is right-handed)
JK_PUBLIC JkMat4 jk_mat4_conversion_from(JkCoordinateSystem source)
{
    JkMat4 result = {0};
    for (int32_t src_axis = 0; src_axis < 3; src_axis++) {
        int32_t dest_axis = source.direction[src_axis] / 2;
        b32 negative = source.direction[src_axis] % 2;
        result.e[dest_axis][src_axis] += negative ? -1 : 1;
    }
    result.e[3][3] = 1;
    return result;
}

// Conversion matrix from my preferred coordinate system, x = right, y = forward, z = up, to a given
// destination coordinate system
JK_PUBLIC JkMat4 jk_mat4_conversion_to(JkCoordinateSystem dest)
{
    JkMat4 result = {0};
    for (int32_t dest_axis = 0; dest_axis < 3; dest_axis++) {
        int32_t src_axis = dest.direction[dest_axis] / 2;
        b32 negative = dest.direction[dest_axis] % 2;
        result.e[dest_axis][src_axis] += negative ? -1 : 1;
    }
    result.e[3][3] = 1;
    return result;
}

JK_PUBLIC JkMat4 jk_mat4_conversion_from_to(JkCoordinateSystem source, JkCoordinateSystem dest)
{
    JkMat4 result = {0};
    for (int32_t src_axis = 0; src_axis < 3; src_axis++) {
        int32_t value = 1;
        int32_t src_dir_index = source.direction[src_axis] / 2;
        int32_t dest_axis;
        for (dest_axis = 0; dest_axis < 3; dest_axis++) {
            int32_t dest_dir_index = dest.direction[dest_axis] / 2;
            if (dest_dir_index == src_dir_index) {
                if (source.direction[src_axis] % 2 != dest.direction[dest_axis] % 2) {
                    value = -1;
                }
                break;
            }
        }
        JK_DEBUG_ASSERT(dest_axis < 3);
        result.e[dest_axis][src_axis] += value;
    }
    result.e[3][3] = 1;
    return result;
}

// ---- JkMat4 end -------------------------------------------------------------

// ---- Shapes begin -----------------------------------------------------------

JK_PUBLIC float jk_segment_y_intersection(JkSegment segment, float y)
{
    float delta_y = segment.p2.y - segment.p1.y;
    JK_ASSERT(delta_y != 0);
    return ((segment.p2.x - segment.p1.x) / delta_y) * (y - segment.p1.y) + segment.p1.x;
}

JK_PUBLIC float jk_segment_x_intersection(JkSegment segment, float x)
{
    float delta_x = segment.p2.x - segment.p1.x;
    JK_ASSERT(delta_x != 0);
    return ((segment.p2.y - segment.p1.y) / delta_x) * (x - segment.p1.x) + segment.p1.y;
}

JK_PUBLIC JkEdge jk_points_to_edge(JkVec2 a, JkVec2 b)
{
    JkEdge edge;
    if (a.y < b.y) {
        edge.segment.p1 = a;
        edge.segment.p2 = b;
        edge.direction = -1.0f;
    } else {
        edge.segment.p1 = b;
        edge.segment.p2 = a;
        edge.direction = 1.0f;
    }
    return edge;
}

JK_PUBLIC JkRect jk_rect(JkVec2 position, JkVec2 dimensions)
{
    return (JkRect){position, jk_vec2_add(position, dimensions)};
}

JK_PUBLIC JkIntVec2 jk_int_rect_dimensions(JkIntRect rect)
{
    return (JkIntVec2){rect.max.x - rect.min.x, rect.max.y - rect.min.y};
}

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVec2 point)
{
    return rect.min.x <= point.x && point.x < rect.max.x && rect.min.y <= point.y
            && point.y < rect.max.y;
}

JK_PUBLIC JkIntRect jk_int_rect_intersect(JkIntRect a, JkIntRect b)
{
    return (JkIntRect){
        .min.x = JK_MAX(a.min.x, b.min.x),
        .min.y = JK_MAX(a.min.y, b.min.y),
        .max.x = JK_MIN(a.max.x, b.max.x),
        .max.y = JK_MIN(a.max.y, b.max.y),
    };
}

JK_PUBLIC JkIntRect jk_triangle2_int_bounding_box(JkTriangle2 t)
{
    return (JkIntRect){
        .min.x = JK_MIN3(t.v[0].x, t.v[1].x, t.v[2].x),
        .min.y = JK_MIN3(t.v[0].y, t.v[1].y, t.v[2].y),
        .max.x = jk_ceil_f32(JK_MAX3(t.v[0].x, t.v[1].x, t.v[2].x)),
        .max.y = jk_ceil_f32(JK_MAX3(t.v[0].y, t.v[1].y, t.v[2].y)),
    };
}

JK_PUBLIC JkEdgeArray jk_triangle2_edges_get(JkArena *arena, JkTriangle2 t)
{
    JkEdgeArray result = {.items = jk_arena_pointer_current(arena)};
    for (int64_t i = 0; i < 3; i++) {
        int64_t next = (i + 1) % 3;
        if (t.v[i].y != t.v[next].y) {
            JkEdge *edge = jk_arena_push(arena, JK_SIZEOF(*edge));
            *edge = jk_points_to_edge(t.v[i], t.v[next]);
        }
    }
    result.count = (JkEdge *)jk_arena_pointer_current(arena) - result.items;
    return result;
}

// ---- Shapes end -------------------------------------------------------------

// ---- Random generator begin -------------------------------------------------

// Bob Jenkins's pseudorandom number generator aka JSF64 from
// https://burtleburtle.net/bob/rand/talksmall.html

static uint64_t jk_rotate_left(uint64_t value, int64_t shift)
{
    JK_DEBUG_ASSERT(0 <= shift && shift < 64);
    return (value << shift) | (value >> (64 - shift));
}

JK_PUBLIC JkRandomGeneratorU64 jk_random_generator_new_u64(uint64_t seed)
{
    JkRandomGeneratorU64 g = {
        g.a = 0xf1ea5eed,
        g.b = seed,
        g.c = seed,
        g.d = seed,
    };

    for (int64_t i = 0; i < 20; i++) {
        jk_random_u64(&g);
    }

    return g;
}

JK_PUBLIC uint64_t jk_random_u64(JkRandomGeneratorU64 *g)
{
    uint64_t e = g->a - jk_rotate_left(g->b, 7);
    g->a = g->b ^ jk_rotate_left(g->c, 13);
    g->b = g->c + jk_rotate_left(g->d, 37);
    g->c = g->d + e;
    g->d = e + g->a;
    return g->d;
}

// ---- Random generator end ---------------------------------------------------

JK_PUBLIC JkColor jk_color_alpha_blend(JkColor foreground, JkColor background, uint8_t alpha)
{
    JkColor result = {0, 0, 0, 255};
    for (uint8_t i = 0; i < 3; i++) {
        result.v[i] = ((int32_t)foreground.v[i] * (int32_t)alpha
                              + background.v[i] * (255 - (int32_t)alpha))
                / 255;
    }
    return result;
}

JK_PUBLIC JkColor jk_color_disjoint_over(JkColor fg, JkColor bg)
{
    JkColor result;

    if (fg.a == 0) {
        result = bg;
    } else if (fg.a == 255) {
        result = fg;
    } else {
        uint8_t bg_a = JK_MIN(bg.a, 255 - fg.a);
        result.a = fg.a + bg_a;
        for (uint32_t i = 0; i < 3; i++) {
            result.v[i] =
                    (uint8_t)(((uint32_t)fg.v[i] * fg.a + (uint32_t)bg.v[i] * bg_a) / result.a);
        }
    }

    return result;
}

JK_NOINLINE JK_PUBLIC void jk_panic(void)
{
    for (;;) {
    }
}

JK_PUBLIC void jk_assert_failed(char *message, char *file, int64_t line)
{
    static uint8_t jk_assert_msg_buf[4096];
    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root,
            (JkBuffer){.size = JK_SIZEOF(jk_assert_msg_buf), .data = jk_assert_msg_buf});
    jk_print(JK_FORMAT(&arena,
            jkfn("Assertion failed: "),
            jkfn(message),
            jkfn(", "),
            jkfn(file),
            jkfn(":"),
            jkfu(line),
            jkf_nl));
    jk_panic();
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
    for (int64_t i = jk_strlen(string) - 1; i >= 0; i--) {
        if (jk_char_is_digit(string[i])) {
            result += (string[i] - '0') * multiplier;
            multiplier *= 10;
        } else if (!(string[i] == ',' || string[i] == '\'' || string[i] == '_')) {
            // Error if character is not a digit or one of the permitted separators
            return -1;
        }
    }
    return result;
}

JK_PUBLIC void *jk_memset(void *address, uint8_t value, int64_t size)
{
    JK_DEBUG_ASSERT(0 <= size);
    uint8_t *bytes = address;
    for (int64_t i = 0; i < size; i++) {
        bytes[i] = value;
    }
    return address;
}

JK_PUBLIC void *jk_memcpy(void *dest, void *src, int64_t size)
{
    JK_DEBUG_ASSERT(0 <= size);
    uint8_t *dest_bytes = dest;
    uint8_t *src_bytes = src;
    for (int64_t i = 0; i < size; i++) {
        dest_bytes[i] = src_bytes[i];
    }
    return dest;
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

// clang-format off
JK_PUBLIC uint8_t jk_bit_reverse_table[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};
// clang-format on

JK_PUBLIC uint16_t jk_bit_reverse_u16(uint16_t value)
{
    return ((uint16_t)jk_bit_reverse_table[value & 0xff] << 8) | jk_bit_reverse_table[value >> 8];
}

JK_PUBLIC uint64_t jk_signed_shift(uint64_t value, int64_t amount)
{
    if (amount < 0) {
        return amount <= -64 ? 0 : value >> -amount;
    } else {
        return 64 <= amount ? 0 : value << amount;
    }
}

JK_PUBLIC b32 jk_is_power_of_two(int64_t x)
{
    return x && (x & (x - 1)) == 0;
}

// Rounds up to nearest power of 2. Leaves 0 as 0.
JK_PUBLIC int64_t jk_round_up_to_power_of_2(int64_t x)
{
    if (x <= 0) {
        return 0;
    } else {
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        x++;
        return x;
    }
}

// Rounds down to nearest power of 2. Leaves 0 as 0.
JK_PUBLIC int64_t jk_round_down_to_power_of_2(int64_t x)
{
    if (x <= 0) {
        return 0;
    } else {
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        x -= x >> 1;
        return x;
    }
}

JK_PUBLIC int32_t jk_round(float value)
{
    return (int32_t)(value + 0.5f);
}

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance)
{
    return JK_ABS(b - a) < tolerance;
}

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance)
{
    return JK_ABS(b - a) < tolerance;
}
