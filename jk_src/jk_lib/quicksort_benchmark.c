#include <stdlib.h>
#include <time.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define ARRAY_LENGTH 20000000
int array[ARRAY_LENGTH];

static int int_compare(void *a, void *b)
{
    return *(int *)a - *(int *)b;
}

int main(void)
{
    srand(clock());

    for (int i = 0; i < ARRAY_LENGTH; i++) {
        array[i] = rand();
    }

    int tmp;
    jk_platform_profile_begin();
    jk_quicksort(array, ARRAY_LENGTH, sizeof(int), &tmp, int_compare);
    jk_platform_profile_end_and_print();
}
