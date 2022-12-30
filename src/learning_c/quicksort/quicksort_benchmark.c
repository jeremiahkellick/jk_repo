#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "quicksort.h"

#define ARRAY_LENGTH 200000

int main(void)
{
    srand(clock());

    int array[ARRAY_LENGTH];

    for (int i = 0; i < ARRAY_LENGTH; i++) {
        array[i] = rand() % 1000;
    }

    double start_time = clock();
    quicksort(array, ARRAY_LENGTH);
    double end_time = clock();
    printf("seconds\n%.3f\n", (end_time - start_time) / CLOCKS_PER_SEC);
}
