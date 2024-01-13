#include "hash_table.h"
#include <assert.h>

#define SMALL_CAPACITY 4
#define SMALL_CAPACITY_LIMIT (SMALL_CAPACITY * JK_HASH_TABLE_LOAD_FACTOR / 10)

#define MEDIUM_CAPACITY 16

static void basic(void)
{
    JkHashTable *t = jk_hash_table_create();
    JkHashTableValue *value;

    assert(t);

    jk_hash_table_put(t, 50, 100);
    jk_hash_table_put(t, 1, 2);
    jk_hash_table_put(t, 365796, 731592);
    jk_hash_table_put(t, 6, 12);
    jk_hash_table_put(t, 12, 24);

    assert(value = jk_hash_table_get(t, 50));
    assert(*value == 100);
    assert(value = jk_hash_table_get(t, 1));
    assert(*value == 2);
    assert(value = jk_hash_table_get(t, 365796));
    assert(*value == 731592);
    assert(value = jk_hash_table_get(t, 6));
    assert(*value == 12);
    assert(value = jk_hash_table_get(t, 12));
    assert(*value == 24);

    assert(jk_hash_table_remove(t, 365796));
    assert(jk_hash_table_remove(t, 50));
    assert(jk_hash_table_remove(t, 6));

    assert(!jk_hash_table_remove(t, 12345));

    assert(!jk_hash_table_get(t, 50));
    assert(!jk_hash_table_get(t, 365796));
    assert(!jk_hash_table_get(t, 6));

    assert(value = jk_hash_table_get(t, 1));
    assert(*value == 2);
    assert(value = jk_hash_table_get(t, 12));
    assert(*value == 24);

    jk_hash_table_put(t, 6, 123);
    assert(value = jk_hash_table_get(t, 6));
    assert(*value == 123);

    value = jk_hash_table_get_with_default(t, 9, 11);
    *value += 11;
    assert(value = jk_hash_table_get(t, 9));
    assert(*value == 22);

    assert(jk_hash_table_remove(t, 9));
    value = jk_hash_table_get_with_default(t, 9, 44);
    *value += 44;
    assert(value = jk_hash_table_get(t, 9));
    assert(*value == 88);

    jk_hash_table_destroy(t);
}

static void resize(void)
{
    JkHashTable *t = jk_hash_table_create();
    JkHashTableValue *value;

    assert(t);

    for (int i = 0; i < 1000; i++) {
        jk_hash_table_put(t, i, i);
    }

    for (int i = 0; i < 1000; i++) {
        assert(value = jk_hash_table_get(t, i));
        assert(*value == (uint32_t)i);
    }

    jk_hash_table_destroy(t);
}

static void get_with_default_triggers_resize(void)
{
    JkHashTable *t = jk_hash_table_create_capacity(SMALL_CAPACITY);
    JkHashTableValue *value;

    assert(t);

    // Fill up to limit
    for (int i = 0; i < SMALL_CAPACITY_LIMIT; i++) {
        jk_hash_table_put(t, i, i);
    }

    // Triggers resize
    value = jk_hash_table_get_with_default(t, 999, 888);
    assert(value == jk_hash_table_get(t, 999));

    jk_hash_table_destroy(t);
}

static void tombstone_cleanup(void)
{
    JkHashTable *t = jk_hash_table_create_capacity(MEDIUM_CAPACITY);
    JkHashTableValue *value;

    assert(t);

    jk_hash_table_put(t, 0, 123);

    for (int i = 2; i < MEDIUM_CAPACITY * 8; i++) {
        if (i % 2 == 0) {
            jk_hash_table_put(t, i, i);
        } else {
            jk_hash_table_remove(t, i - 1);
        }
    }

    assert(t->capacity == MEDIUM_CAPACITY);
    assert(value = jk_hash_table_get(t, 0));
    assert(*value == 123);

    jk_hash_table_destroy(t);
}

int main(void)
{
    basic();
    resize();
    get_with_default_triggers_resize();
    tombstone_cleanup();
    return 0;
}
