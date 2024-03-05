#include <stdio.h>

#include "dep2.h"

// #jk_build dependencies_begin
#include <jk_src/jk_build/tests/dependencies/dep_nested.h>
// #jk_build dependencies_end

void dep2(void)
{
    printf("Successfully called dependency 2/3\n");
    dep_nested();
}
