#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unordered_map>

using namespace std;

constexpr int NUM_ELEMENTS = 46'976'204;
constexpr int SEED = 1608690770;

int main()
{
    unordered_map<int, int> map;
    int sum = 0;
    clock_t start_time;
    clock_t end_time;

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        map[rand()] = i & 0xf;
    }
    end_time = clock();
    printf("Writes w/ resize: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        sum += map[rand()];
    }
    end_time = clock();
    printf("Reads: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        int num = rand();
        if (map.contains(num)) {
            sum += map[num];
        }
    }
    end_time = clock();
    printf("Misses: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        map.erase(rand());
    }
    end_time = clock();
    printf("Removes: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        map[rand()] = i & 0xf;
    }
    end_time = clock();
    printf("Writes: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    // Somehow prevents a compiler "optimization" from slowing down the reads timing
    for (int i = 0; i < 10; i++) {
        int num = rand();
        if (map.contains(num)) {
            sum += map[num];
        }
    }

    printf("Sum: %d\n", sum);

    return 0;
}
