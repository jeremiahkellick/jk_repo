#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool contains_color(unsigned char pixels, unsigned char color_mask) {
  char xord = pixels ^ color_mask;
  char split_res = xord | (xord >> 1);
  return (~split_res & 0x55) != 0;
}

void test_contains_color(unsigned char pixels, unsigned char color, bool result) {
  int color_mask =
    color
    | (color << 2)
    | (color << 4)
    | (color << 6);

  if (contains_color(pixels, color_mask) == result) {
    return;
  }

  printf("Expected %X %sto contain %X\n", pixels, result ? "" : "NOT ", color);
  exit(1);
}

int main(void) {
  test_contains_color(0x6C, 0x1, true);
  test_contains_color(0x47, 0x1, true);
  test_contains_color(0xEC, 0x1, false);

  test_contains_color(0x9C, 0x0, true);
  test_contains_color(0x9D, 0x0, false);

  printf("SUCCESS!\n");
  return 0;
}

