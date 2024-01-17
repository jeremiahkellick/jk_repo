#include <stdio.h>

#include <jk_src/jk_build/tests/dependencies/big_mode.c>
#include <jk_src/jk_build/tests/dependencies/pop_off.c>

int main(void)
{
    printf("Hello, world!\n");
    pop_off();
    return 0;
}
