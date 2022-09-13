#include <stdio.h>

/**
 * Print fahrenheit-celsuis table
 * for fahrenheit = 20, 40, 60, ..., 300
 */
int main(void)
{
  printf("Fahrenheit\tCelsius\n");
  for (int fahrenheit = 0; fahrenheit <= 300; fahrenheit += 20) {
    float celsius = 5.0 / 9.0 * (fahrenheit - 32.0);
    printf("%10d\t%7.1f\n", fahrenheit, celsius);
  }
  return 0;
}
