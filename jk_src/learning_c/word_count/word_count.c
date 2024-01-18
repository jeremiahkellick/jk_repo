#include <stdio.h>

#include <jk_src/learning_c/word_count/file_get_word_count.c>

int main(void)
{
    printf("%d\n", file_get_word_count(stdin));
    return 0;
}
