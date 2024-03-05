#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_build/tests/dependencies/dep1.h>
#include <jk_src/jk_build/tests/dependencies/dep2.h>
// #jk_build dependencies_end

int main(void)
{
    dep1();
    dep2();
    return 0;
}
