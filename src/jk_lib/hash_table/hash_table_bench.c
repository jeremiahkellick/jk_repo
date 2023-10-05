#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_ELEMENTS 50000000

int main(void)
{
    JkHashTable *t = jk_hash_table_create();

    clock_t start_time = clock();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        (*jk_hash_table_get_with_default(t, rand(), 0))++;
    }
    clock_t end_time = clock();

    printf("%.2f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    jk_hash_table_destroy(t);

    return 0;
}
