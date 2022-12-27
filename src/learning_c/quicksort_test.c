#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARRAY_LENGTH 200000

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
