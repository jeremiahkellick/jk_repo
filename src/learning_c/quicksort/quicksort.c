#include <stdlib.h>
#include <string.h>

#include "quicksort.h"

static void swap(void *a, void *b, int element_size, void *tmp)
{
    memcpy(tmp, a, element_size);
    memcpy(a, b, element_size);
    memcpy(b, tmp, element_size);
}

void quicksort(void *array_void,
        int element_count,
        int element_size,
        void *tmp,
        int (*compare)(void *a, void *b))
{
    if (element_count < 2) {
        return;
    }

    char *array = array_void;
    char *low = array;
    char *mid = array + element_size;
    char *high = array + (element_count - 1) * element_size;

    // Move random element to start to use as pivot
    char *random_element = array + (rand() % element_count) * element_size;
    swap(array, random_element, element_size, tmp);

    while (mid <= high) {
        // Compare mid with pivot. Pivot is always 1 element before mid.
        int comparison = (*compare)(mid, mid - element_size);

        if (comparison < 0) {
            swap(low, mid, element_size, tmp);
            low += element_size;
            mid += element_size;
        } else if (comparison > 0) {
            swap(mid, high, element_size, tmp);
            high -= element_size;
        } else {
            mid += element_size;
        }
    }

    int left_count = (int)(low - array) / element_size;
    int right_count = element_count - (int)(mid - array) / element_size;
    quicksort(array, left_count, element_size, tmp, compare);
    quicksort(mid, right_count, element_size, tmp, compare);
}

static int int_compare(int *a, int *b)
{
    return *a - *b;
}

void quicksort_ints(int *array, int length)
{
    int tmp;
    quicksort(array, length, sizeof(int), &tmp, int_compare);
}

static int string_compare(char **a, char **b)
{
    return strcmp(*a, *b);
}

void quicksort_strings(char **array, int length)
{
    char *tmp;
    quicksort(array, length, sizeof(char *), &tmp, string_compare);
}
