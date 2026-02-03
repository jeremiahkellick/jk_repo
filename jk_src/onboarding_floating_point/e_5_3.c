#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

// clang-format off
uint32_t values[] =
{
    0x012763c0, 0x0135f328, 0x0071e438, 0x012b9f10,
    0x00be5ea8, 0x0001007c, 0x00010098, 0x000101b3,
    0x00147dc8, 0x00904d60, 0x0000ffd0, 0x0058b330,
    0x00010149, 0x000101a0, 0x0001011c, 0x0000806a,
    0x00010139, 0x000101e5, 0x01a7a068, 0x0001012e,
    0x0001012f, 0x00fa2f00, 0x000080f5, 0x00000000,
    0x00003c78, 0x00000333, 0x000006cc, 0x00000001,
    0x00001895, 0x00000001, 0x0000037a, 0x00000005,
    0x00000000, 0x0000000c, 0x000009d5, 0x0000003e,
    0x000000fb, 0x00000004, 0x000018c6, 0x000000ff,
    0x00000042, 0x00000263, 0x00000ba4, 0x000038d1,
    0x00000e69, 0x00000039
};
// clang-format on

char *file_name = "e_5_3_table.csv";

static int8_t most_significant_bit_index(uint32_t value)
{
    for (uint8_t i = 30; i; i--) {
        if ((value >> i) & 1) {
            return i;
        }
    }
    return 0;
}

static uint32_t signed_shift(uint32_t value, int8_t amount)
{
    if (amount < 0) {
        return value >> -amount;
    } else {
        return value << amount;
    }
}

static uint16_t compress(uint32_t value)
{
    int8_t exponent = most_significant_bit_index(value);
    uint16_t sign = (value >> 16) & 0x8000;
    uint16_t mantissa = signed_shift(value, 10 - JK_MAX(1, exponent)) & 0x3ff;
    return sign | ((uint16_t)exponent << 10) | mantissa;
}

static double s10e5_to_double(uint16_t value)
{
    double sign = (value & 0x8000) ? -1.0 : 1.0;
    uint16_t mantissa = value & 0x3ff;

    int8_t exponent_raw = (value >> 10) & 0x1f;
    int8_t exponent = JK_MAX(1, exponent_raw) - 25;
    if (exponent_raw) {
        mantissa |= 0x400;
    }

    return sign * mantissa * pow(2.0, (double)exponent);
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, JK_GIGABYTE);

    FILE *file = fopen(file_name, "wb");
    if (file) {
        fprintf(file, "decimal,1:15:16,s10e5,approx\n");
        for (uint64_t i = 0; i < JK_ARRAY_COUNT(values); i++) {
            uint8_t sign = (values[i] >> 31) & 1;
            uint16_t whole = (values[i] >> 15) & 0xffff;
            uint16_t fractional = values[i] & 0x7fff;

            uint16_t s10e5 = compress(values[i]);
            uint8_t fsign = (s10e5 >> 15) & 1;
            uint8_t exponent = (s10e5 >> 10) & 0x1f;
            uint16_t mantissa = s10e5 & 0x3ff;

            double decimal =
                    (sign ? -1.0 : 1.0) * (double)(values[i] & 0x7fffffff) / (32.0 * 1024.0);
            double approx = s10e5_to_double(s10e5);

            // clang-format off
            JkBuffer fixed_point = JK_FORMAT(&arena,
                    jkfb(sign, 1), jkfn(":"), jkfb(whole, 16), jkfn(":"), jkfb(fractional, 15));
            // clang-format on

            // clang-format off
            JkBuffer floating_point = JK_FORMAT(&arena,
                    jkfb(fsign, 1), jkfn(":"), jkfb(exponent, 5), jkfn(":"), jkfb(mantissa, 10));
            // clang-format on

            fprintf(file,
                    "%.6f,%.*s,%.*s,%.6f\n",
                    decimal,
                    (int)fixed_point.size,
                    fixed_point.data,
                    (int)floating_point.size,
                    floating_point.data,
                    approx);
        }
    } else {
        fprintf(stderr, "%s: Failed to open '%s': %s", argv[0], file_name, strerror(errno));
    }

    return 0;
}
