#include <jk_gen/jk_build/tests/single_translation_unit_test.stu.h>

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
