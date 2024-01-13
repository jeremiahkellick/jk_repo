#include <stdio.h>

#include "big_mode.h"
#include "pop_off.h"

void pop_off(void)
{
    for (int i = 0; i < 5; i++) {
        printf("I'm popping off!\n");
    }
    big_mode();
}
