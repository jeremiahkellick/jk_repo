#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "quicksort.h"

#define LENGTH 10

static int numbers[LENGTH] = {40, 34, 49, 28, 4, 42, 30, 27, 23, 12};
static int sorted_numbers[LENGTH] = {4, 12, 23, 27, 28, 30, 34, 40, 42, 49};

static char *strings[LENGTH] = {
        "Pear",
        "Mango",
        "Strawberry",
        "Watermelon",
        "Orange",
        "Pineapple",
        "Banana",
        "Blueberry",
        "Kiwi",
        "Apple",
};

static char *sorted_strings[LENGTH] = {
        "Apple",
        "Banana",
        "Blueberry",
        "Kiwi",
        "Mango",
        "Orange",
        "Pear",
        "Pineapple",
        "Strawberry",
        "Watermelon",
};

static bool string_arrays_are_equal(char **a, char **b, int length)
{
    for (int i = 0; i < length; i++) {
        if (strcmp(a[i], b[i]) != 0) {
            return false;
        }
    }
    return true;
}

static void print_int_array(int array[])
{
    printf("{");
    for (int i = 0; i < LENGTH; i++) {
        printf("%d%s", array[i], i == 9 ? "}" : ", ");
    }
}

static void print_string_arrays_side_by_side(
        char **a, char **b, int length, int indent, int width)
{
    for (int i = 0; i < length; i++) {
        // Indent
        for (int j = 0; j < indent; j++) {
            putchar(' ');
        }

        printf("%s", a[i]);

        // Pad to width with spaces
        int padding = width - (int)strlen(a[i]);
        for (int j = 0; j < padding; j++) {
            putchar(' ');
        }

        printf("%s\n", b[i]);
    }
}

int main(void)
{
    // Test int sorting
    printf("quicksort_ints()\n    ");
    quicksort_ints(numbers, LENGTH);
    if (memcmp(numbers, sorted_numbers, LENGTH) == 0) {
        printf("SUCCESS\n");
    } else {
        printf("FAIL:\n        expected ");
        print_int_array(sorted_numbers);
        printf("\n        actual   ");
        print_int_array(numbers);
        printf("\n");
    }

    // Test string sorting
    printf("\nquicksort_strings()\n");
    quicksort_strings(strings, LENGTH);
    if (string_arrays_are_equal(strings, sorted_strings, LENGTH)) {
        printf("    SUCCESS\n");
    } else {
        printf("    FAIL:\n        expected    actual\n\n");
        print_string_arrays_side_by_side(
                sorted_strings, strings, LENGTH, 8, 12);
    }

    return 0;
}
