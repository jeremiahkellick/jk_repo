
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
void quicksort(void *array,
        int element_count,
        int element_size,
        void *tmp,
        int (*compare)(void *a, void *b));

void quicksort_ints(int *array, int length);

void quicksort_strings(char **array, int length);
