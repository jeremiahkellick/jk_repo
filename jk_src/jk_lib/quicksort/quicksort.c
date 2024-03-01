#ifndef JK_QUICKSORT_C
#define JK_QUICKSORT_C

#include <stdlib.h>
#include <string.h>

#include "quicksort.h"

static void jk_bytes_swap(void *a, void *b, int element_size, void *tmp)
{
    memcpy(tmp, a, element_size);
    memcpy(a, b, element_size);
    memcpy(b, tmp, element_size);
}

/**
 * @brief Sorts elements in the given array.
 *
 * See the definition of quicksort_ints or quicksort_strings for example usage.
 *
 * @param array pointer to the start of the array
 * @param element_count number of elements in the array
 * @param element_size size of a single element in bytes
 * @param tmp pointer to temporary storage the size of a single element
 * @param compare Function used to compare two elements given pointers to them.
 *     Return less than zero if a should come before b, greater than zero if a
 *     should come after b, and zero if they are equal.
 */
void jk_quicksort(void *array_void,
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
    jk_bytes_swap(array, random_element, element_size, tmp);

    while (mid <= high) {
        // Compare mid with pivot. Pivot is always 1 element before mid.
        int comparison = compare(mid, mid - element_size);

        if (comparison < 0) {
            jk_bytes_swap(low, mid, element_size, tmp);
            low += element_size;
            mid += element_size;
        } else if (comparison > 0) {
            jk_bytes_swap(mid, high, element_size, tmp);
            high -= element_size;
        } else {
            mid += element_size;
        }
    }

    int left_count = (int)(low - array) / element_size;
    int right_count = element_count - (int)(mid - array) / element_size;
    jk_quicksort(array, left_count, element_size, tmp, compare);
    jk_quicksort(mid, right_count, element_size, tmp, compare);
}

static int jk_int_compare(void *a, void *b)
{
    return *(int *)a - *(int *)b;
}

void jk_quicksort_ints(int *array, int length)
{
    int tmp;
    jk_quicksort(array, length, sizeof(int), &tmp, jk_int_compare);
}

static int jk_string_compare(void *a, void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

void jk_quicksort_strings(char **array, int length)
{
    char *tmp;
    jk_quicksort(array, length, sizeof(char *), &tmp, jk_string_compare);
}

#endif
