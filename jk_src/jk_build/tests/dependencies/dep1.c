#ifndef MTU_DEP1_C
#define MTU_DEP1_C

#include <stdio.h>

#include "mtu_dep1.h"

// #jk_build dependencies_adjacent
#include <jk_src/jk_build/tests/dependencies/mtu_dep2.h>
// #jk_build end

void dep1(void)
{
    printf("Successfully called dependency 1/2\n");
    dep2();
}

#endif
