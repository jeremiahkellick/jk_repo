#include <stdint.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

static uint8_t reverse_table[256];

int main(void)
{
    for (uint64_t index = 0; index < JK_ARRAY_COUNT(reverse_table); index++) {
        uint8_t byte = (uint8_t)index;
        reverse_table[index] = __builtin_bitreverse8(byte);
    }

    printf("JK_PUBLIC uint8_t jk_bit_reverse_table[256] = {\n");
    uint64_t i = 0;
    while (i < JK_ARRAY_COUNT(reverse_table)) {
        printf("    ");
        for (uint64_t rep = 0; i < JK_ARRAY_COUNT(reverse_table) && rep < 8; rep++) {
            printf("0x%02x,%c", reverse_table[i++], rep == 7 ? '\n' : ' ');
        }
    }
    printf("};\n");
}
