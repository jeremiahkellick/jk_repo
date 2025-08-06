#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

// clang-format off
uint32_t values[] =
{
    0x012763c0, 0x0135f328, 0x0071e438, 0x012b9f10,
    0x00be5ea8, 0x0001007c, 0x00010098, 0x000101b3,
    0x00147dc8, 0x00904d60, 0x0000ffd0, 0x0058b330,
    0x00010149, 0x000101a0, 0x0001011c, 0x0000806a,
    0x00010139, 0x000101e5, 0x01a7a068, 0x0001012e,
    0x0001012f, 0x00fa2f00, 0x000080f5
};
// clang-format on

char *file_name = "e_5_1_table.csv";

static uint8_t most_significant_bit_index(uint32_t value)
{
    if (value == 0) {
        return UINT8_MAX;
    }
    for (uint8_t i = 31; i; i--) {
        if ((value >> i) & 1) {
            return i;
        }
    }
    return 0;
}

static uint32_t mask(uint32_t value)
{
    uint8_t msb = most_significant_bit_index(value);
    if (10 < msb) {
        return (int32_t)0x80000000 >> (31 - (msb - 10));
    } else {
        return 0xffffffff;
    }
}

static void print_binary(FILE *file, uint32_t value, uint8_t digit_count)
{
    uint32_t mask = 1 << (digit_count - 1);
    while (mask) {
        putc(value & mask ? '1' : '0', file);
        mask >>= 1;
    }
}

static void print_value(FILE *file, uint32_t value)
{
    print_binary(file, value >> 31, 1);
    fprintf(file, ":");
    print_binary(file, value >> 15, 16);
    fprintf(file, ":");
    print_binary(file, value, 15);
}

int main(int argc, char **argv)
{
    FILE *file = fopen(file_name, "wb");
    if (file) {
        fprintf(file, "approx,d (bin),d_masked (bin),approx\n");
        for (uint64_t i = 0; i < JK_ARRAY_COUNT(values); i++) {
            fprintf(file, "%.6f,", (double)values[i] / 32768.0);
            print_value(file, values[i]);
            fprintf(file, ",");

            uint32_t masked_value = values[i] & mask(values[i]);
            print_value(file, masked_value);
            fprintf(file, ",");
            fprintf(file, "%.6f", (double)masked_value / 32768.0);

            fprintf(file, "\n");
        }
    } else {
        fprintf(stderr, "%s: Failed to open '%s': %s", argv[0], file_name, strerror(errno));
    }
}
