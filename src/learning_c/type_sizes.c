#include <stdio.h>

int main(void)
{
    printf("char\t%ld\n", (int)sizeof(char) * 8);
    printf("short\t%ld\n", (int)sizeof(short) * 8);
    printf("int\t%ld\n", (int)sizeof(int) * 8);
    printf("long\t%ld\n", (int)sizeof(long) * 8);
    printf("float\t%ld\n", (int)sizeof(float) * 8);
    printf("double\t%ld\n", (int)sizeof(double) * 8);
}
