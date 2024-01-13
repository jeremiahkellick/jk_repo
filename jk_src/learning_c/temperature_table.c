#include <stdio.h>

#define LOWER 0
#define UPPER 300
#define STEP 20

/**
 * Print fahrenheit-celsuis table
 * for fahrenheit = 20, 40, 60, ..., 300
 */
int main(void)
{
    printf("Fahrenheit\tCelsius\n");
    for (int fahrenheit = LOWER; fahrenheit <= UPPER; fahrenheit += STEP) {
        float celsius = 5.0f / 9.0f * (fahrenheit - 32.0f);
        printf("%10d\t%7.1f\n", fahrenheit, celsius);
    }
    return 0;
}
