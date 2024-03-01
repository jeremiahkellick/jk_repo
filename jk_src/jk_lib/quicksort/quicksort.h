#ifndef JK_QUICKSORT_H
#define JK_QUICKSORT_H

void jk_quicksort(void *array,
        int element_count,
        int element_size,
        void *tmp,
        int (*compare)(void *a, void *b));

void jk_quicksort_ints(int *array, int length);

void jk_quicksort_strings(char **array, int length);

#endif
