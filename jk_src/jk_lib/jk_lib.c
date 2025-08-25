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

JK_PUBLIC uint64_t jk_count_leading_zeros(uint64_t value)
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

JK_PUBLIC uint64_t jk_count_leading_zeros(uint64_t value)
{
    return __builtin_clzl(value);
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
    for (uint64_t i = 0; i < buffer.size / 2; i++) {
        JK_SWAP(buffer.data[i], buffer.data[buffer.size - 1 - i], uint8_t);
    }
}

static uint64_t jk_strlen(char *string)
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

JK_PUBLIC int jk_buffer_character_get(JkBuffer buffer, uint64_t pos)
{
    return pos < buffer.size ? buffer.data[pos] : JK_EOF;
}

JK_PUBLIC int jk_buffer_character_next(JkBuffer buffer, uint64_t *pos)
{
    int c = jk_buffer_character_get(buffer, *pos);
    (*pos)++;
    return c;
}

JK_PUBLIC int jk_buffer_compare(JkBuffer a, JkBuffer b)
{
    for (uint64_t pos = 0; 1; pos++) {
        int a_char = jk_buffer_character_get(a, pos);
        int b_char = jk_buffer_character_get(b, pos);
        if (a_char < b_char) {
            return -1;
        } else if (a_char > b_char) {
            return 1;
        } else if (a_char == JK_EOF && b_char == JK_EOF) {
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
    for (uint64_t i = 0; i < string.size; i++) {
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

JK_PUBLIC JkBuffer jk_f64_to_string(JkArena *arena, double value, uint16_t decimal_places)
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
                for (uint64_t i = 0; i < 8 - decimal_places; i++) {
                    divisor *= 10;
                }

                uint64_t rounded_fraction = (decimal_fraction + divisor / 2) / divisor;
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

    for (uint64_t i = 0; i < items.count; i++) {
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

static b32 jk_arena_fixed_grow(JkArena *arena, uint64_t new_size)
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

JK_PUBLIC void *jk_arena_push(JkArena *arena, uint64_t size)
{
    uint64_t new_pos = arena->pos + size;
    if (arena->root->memory.size < new_pos) {
        if (!arena->grow(arena, new_pos)) {
            return 0;
        }
    }
    void *address = arena->root->memory.data + arena->pos;
    arena->pos = new_pos;
    return address;
}

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, uint64_t size)
{
    void *address = jk_arena_push(arena, size);
    jk_memset(address, 0, size);
    return address;
}

JK_PUBLIC void jk_arena_pop(JkArena *arena, uint64_t size)
{
    JK_DEBUG_ASSERT(size <= arena->pos - arena->base);
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

// ---- Quicksort begin --------------------------------------------------------

static void jk_bytes_swap(void *a, void *b, uint64_t element_size, void *tmp)
{
    jk_memcpy(tmp, a, element_size);
    jk_memcpy(a, b, element_size);
    jk_memcpy(b, tmp, element_size);
}

static void jk_quicksort_internal(JkRandomGeneratorU64 *generator,
        void *array_void,
        uint64_t element_count,
        uint64_t element_size,
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
    char *random_element = array + (jk_random_u64(generator) % element_count) * element_size;
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

    uint64_t left_count = (uint64_t)(low - array) / element_size;
    uint64_t right_count = element_count - (uint64_t)(mid - array) / element_size;
    jk_quicksort_internal(generator, array, left_count, element_size, tmp, compare);
    jk_quicksort_internal(generator, mid, right_count, element_size, tmp, compare);
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
        uint64_t element_count,
        uint64_t element_size,
        void *tmp,
        int (*compare)(void *a, void *b))
{
    JkRandomGeneratorU64 generator = jk_random_generator_new_u64(0x9646e4db8d81f399);
    jk_quicksort_internal(&generator, array_void, element_count, element_size, tmp, compare);
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

JK_PUBLIC JkVector2 jk_vector_2_ceil(JkVector2 v)
{
    return (JkVector2){jk_ceil_f32(v.x), jk_ceil_f32(v.y)};
}

JK_PUBLIC JkIntVector2 jk_vector_2_ceil_i(JkVector2 v)
{
    JkVector2 v_ceiled = jk_vector_2_ceil(v);
    return (JkIntVector2){(int32_t)v_ceiled.x, (int32_t)v_ceiled.y};
}

JK_PUBLIC float jk_vector_2_magnitude_sqr(JkVector2 v)
{
    return v.x * v.x + v.y * v.y;
}

JK_PUBLIC float jk_vector_2_magnitude(JkVector2 v)
{
    return jk_sqrt_f32(jk_vector_2_magnitude_sqr(v));
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
            * jk_acos_f32(
                    jk_vector_2_dot(u, v) / (jk_vector_2_magnitude(u) * jk_vector_2_magnitude(v)));
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

// ---- Random generator begin -------------------------------------------------

// Bob Jenkins's pseudorandom number generator aka JSF64 from
// https://burtleburtle.net/bob/rand/talksmall.html

static uint64_t jk_rotate_left(uint64_t value, uint64_t shift)
{
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

    for (uint64_t i = 0; i < 20; i++) {
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

JK_NOINLINE JK_PUBLIC void jk_panic(void)
{
    for (;;) {
    }
}

JK_PUBLIC void jk_assert_failed(char *message, char *file, int64_t line)
{
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
    for (uint64_t i = jk_strlen(string) - 1; i >= 0; i--) {
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

JK_PUBLIC void *jk_memset(void *address, uint8_t value, uint64_t size)
{
    uint8_t *bytes = address;
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = value;
    }
    return address;
}

JK_PUBLIC void *jk_memcpy(void *dest, void *src, uint64_t size)
{
    uint8_t *dest_bytes = dest;
    uint8_t *src_bytes = src;
    for (uint64_t i = 0; i < size; i++) {
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

JK_PUBLIC b32 jk_int_rect_point_test(JkIntRect rect, JkIntVector2 point)
{
    JkIntVector2 delta = jk_int_vector_2_sub(point, rect.position);
    return 0 <= delta.x && delta.x < rect.dimensions.x && 0 <= delta.y
            && delta.y < rect.dimensions.y;
}

JK_PUBLIC uint64_t jk_signed_shift(uint64_t value, int8_t amount)
{
    if (amount < 0) {
        return amount <= -64 ? 0 : value >> -amount;
    } else {
        return 64 <= amount ? 0 : value << amount;
    }
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

JK_PUBLIC b32 jk_float32_equal(float a, float b, float tolerance)
{
    return JK_ABS(b - a) < tolerance;
}

JK_PUBLIC b32 jk_float64_equal(double a, double b, double tolerance)
{
    return JK_ABS(b - a) < tolerance;
}
