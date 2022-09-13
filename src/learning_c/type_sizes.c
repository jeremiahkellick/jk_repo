#include <stdio.h>

int main(void)
{
  printf("char\t%ld\n", sizeof(char) * 8);
  printf("short\t%ld\n", sizeof(short) * 8);
  printf("int\t%ld\n", sizeof(int) * 8);
  printf("long\t%ld\n", sizeof(long) * 8);
  printf("float\t%ld\n", sizeof(float) * 8);
  printf("double\t%ld\n", sizeof(double) * 8);
}
