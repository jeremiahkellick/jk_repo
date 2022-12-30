#include <stdio.h>
#include <string.h>

#include "quicksort.h"

#define LENGTH 10

static int numbers[LENGTH] = {40, 34, 49, 28, 4, 42, 30, 27, 23, 12};
static int sorted_numbers[LENGTH] = {4, 12, 23, 27, 28, 30, 34, 40, 42, 49};

static void print_array(int array[])
{
    printf("{");
    for (int i = 0; i < LENGTH; i++) {
        printf("%d%s", array[i], i == 9 ? "}" : ", ");
    }
}

int main(void)
{
    printf("quicksort()\n    ");
    quicksort(numbers, LENGTH);
    if (memcmp(numbers, sorted_numbers, LENGTH) == 0) {
        printf("SUCCESS\n");
    } else {
        printf("FAIL:\n        expected ");
        print_array(sorted_numbers);
        printf("\n        actual   ");
        print_array(numbers);
        printf("\n");
    }
    return 0;
}
