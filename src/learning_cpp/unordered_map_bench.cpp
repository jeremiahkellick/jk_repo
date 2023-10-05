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
        int num = rand() % (NUM_ELEMENTS / 100);
        if (map.contains(num)) {
            map[num]++;
        } else {
            map[num] = 1;
        }
    }
    clock_t end_time = clock();

    printf("%.2f\n\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    for (int i = 0; i < 20; i++) {
        if (map.contains(i)) {
            printf("%2d: %3d\n", i, map[i]);
        } else {
            printf("-\n");
        }
    }

    return 0;
}
