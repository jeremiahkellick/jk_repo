#include "quicksort.h"

void quicksort(int array[], int length)
{
    if (length <= 1) {
        return;
    }

    int pivot = array[0];
    int low = 0;
    int mid = 0;
    int high = length - 1;

    while (mid <= high) {
        if (array[mid] < pivot) {
            int tmp = array[low];
            array[low] = array[mid];
            array[mid] = tmp;

            low++;
            mid++;
        } else if (array[mid] > pivot) {
            int tmp = array[high];
            array[high] = array[mid];
            array[mid] = tmp;

            high--;
        } else {
            mid++;
        }
    }

    if (low > 0) {
        quicksort(array, low);
    }
    if (mid < length) {
        quicksort(array + mid, length - mid);
    }
}
