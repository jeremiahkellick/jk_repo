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

char *file_name = "e_5_2_table.csv";

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

static uint16_t compress(uint32_t value, uint8_t msb)
{
    if (msb < 15) {
        return 0;
    } else {
        return (value >> (msb - 10)) & 0x3ff;
    }
}

int main(int argc, char **argv)
{
    FILE *file = fopen(file_name, "wb");
    if (file) {
        for (uint64_t i = 0; i < JK_ARRAY_COUNT(values); i++) {
            uint8_t msb = most_significant_bit_index(values[i]);
            uint16_t compressed = compress(values[i], msb);
            uint32_t decompressed = (1u << (msb - 5)) | (uint32_t)compressed << (msb - 15);
            uint32_t numerator = decompressed % 1024;
            uint32_t integer = decompressed / 1024;
            fprintf(file, "%u + (%u / 1024)\n", integer, numerator);
        }
    } else {
        fprintf(stderr, "%s: Failed to open '%s': %s", argv[0], file_name, strerror(errno));
    }
}
