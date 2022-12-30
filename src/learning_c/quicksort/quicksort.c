#include "quicksort.h"

static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void quicksort(int array[], int length)
{
    if (length < 2) {
        return;
    }

    int low = 0;
    int mid = 1;
    int high = length - 1;

    // Move middle element to start to use as pivot
    swap(array, array + length / 2);
    int pivot = array[0];

    while (mid <= high) {
        if (array[mid] < pivot) {
            swap(array + low++, array + mid++);
        } else if (array[mid] > pivot) {
            swap(array + mid, array + high--);
        } else {
            mid++;
        }
    }

    quicksort(array, low);
    quicksort(array + mid, length - mid);
}
