#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unordered_map>

using namespace std;

constexpr int NUM_ELEMENTS = 50'000'000;

int main()
{
    unordered_map<int, int> map;

    clock_t start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        map[rand()]++;
    }
    clock_t end_time = clock();

    printf("%.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    return 0;
}
