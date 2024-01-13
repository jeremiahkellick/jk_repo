#include <array>
#include <stdio.h>

using namespace std;

constexpr int start = 0;
constexpr int row_count = 16;
constexpr int step = 20;

struct Temperature {
    float fahrenheit;
    float celsuis;
};

constexpr array<Temperature, row_count> build_temperature_table()
{
    array<Temperature, row_count> temperature_table{};
    int current = start;
    for (auto &temp : temperature_table) {
        temp.fahrenheit = (float)current;
        temp.celsuis = 5.0f * (current - 32) / 9.0f;
        current += step;
    }
    return temperature_table;
}

constexpr auto temperature_table = build_temperature_table();

int main()
{
    printf("Fahrenheit\tCelsius\n");
    for (const auto &temp : temperature_table) {
        printf("%10.0f\t%7.1f\n", temp.fahrenheit, temp.celsuis);
    }
    return 0;
}
