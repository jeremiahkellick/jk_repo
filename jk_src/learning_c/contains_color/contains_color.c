#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool contains_color(unsigned char pixels, unsigned char color_mask)
{
    unsigned char bit_compare = ~(pixels ^ color_mask);
    unsigned char byte_result = bit_compare & (bit_compare >> 1) & '\x55';
    return byte_result != 0;
}

void print_byte_as_binary(unsigned char byte)
{
    for (int i = 7; i >= 0; i--) {
        putchar((byte >> i) & 1 ? '1' : '0');
    }
}

void test_contains_color(unsigned char pixels, unsigned char color, bool result)
{
    int color_mask = color | (color << 2) | (color << 4) | (color << 6);

    if (contains_color(pixels, color_mask) == result) {
        return;
    }

    printf("Expected contains_color(");
    print_byte_as_binary(pixels);
    printf(", ");
    print_byte_as_binary(color_mask);
    printf(") to be %s but got %s\n", result ? "true" : "false", result ? "false" : "true");
    exit(1);
}

int main(void)
{
    test_contains_color(0x6C, 0x1, true);
    test_contains_color(0x47, 0x1, true);
    test_contains_color(0xEC, 0x1, false);
    test_contains_color(0x3C, 0x1, false);

    test_contains_color(0x9C, 0x0, true);
    test_contains_color(0x9D, 0x0, false);

    printf("SUCCESS!\n");
    return 0;
}
