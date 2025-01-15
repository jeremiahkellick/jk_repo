#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <jk_gen/jk_lib/hash_table/hash_table_bench.stu.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/hash_table/hash_table.h>
// #jk_build dependencies_end

#define CAPACITY (1 << 26)
#define NUM_ELEMENTS (CAPACITY * JK_HASH_TABLE_LOAD_FACTOR / 10)
#define SEED 1608690770

int main(void)
{
    JkHashTable *t = jk_hash_table_create();
    int sum = 0;
    clock_t start_time;
    clock_t end_time;

    printf("NUM_ELEMENTS: %d\n", NUM_ELEMENTS);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        jk_hash_table_put(t, rand(), i & 0xf);
    }
    end_time = clock();
    printf("Writes w/ resize: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        sum += *jk_hash_table_get(t, rand());
    }
    end_time = clock();
    printf("Reads: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        JkHashTableValue *value = jk_hash_table_get(t, rand());
        if (value) {
            sum += *value;
        }
    }
    end_time = clock();
    printf("Misses: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        jk_hash_table_remove(t, rand());
    }
    end_time = clock();
    printf("Removes: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    srand(SEED);
    start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        jk_hash_table_put(t, rand(), i & 0xf);
    }
    end_time = clock();
    printf("Writes: %.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    // Somehow prevents a compiler "optimization" from slowing down the reads timing
    for (int i = 0; i < 10; i++) {
        JkHashTableValue *value = jk_hash_table_get(t, rand());
        if (value) {
            sum += *value;
        }
    }

    printf("Sum: %d\n", sum);

    jk_hash_table_destroy(t);

    return 0;
}
