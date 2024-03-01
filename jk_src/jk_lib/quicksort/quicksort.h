#ifndef JK_QUICKSORT_H
#define JK_QUICKSORT_H

#include <stddef.h>

void jk_quicksort(void *array,
        size_t element_count,
        size_t element_size,
        void *tmp,
        int (*compare)(void *a, void *b));

void jk_quicksort_ints(int *array, int length);

void jk_quicksort_strings(char **array, int length);

#endif
