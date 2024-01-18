#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <jk_src/learning_c/quicksort/quicksort.c>

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
    double start_time = clock();
    jk_quicksort(array, ARRAY_LENGTH, sizeof(int), &tmp, int_compare);
    double end_time = clock();
    printf("seconds\n%.3f\n", (end_time - start_time) / CLOCKS_PER_SEC);
}
