#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/learning_c/word_count/file_get_word_count.h>
// #jk_build dependencies_end

int main(void)
{
    printf("%d\n", file_get_word_count(stdin));
    return 0;
}
