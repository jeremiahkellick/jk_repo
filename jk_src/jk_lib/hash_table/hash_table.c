#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/utils.h>
// #jk_build dependencies_end

#include "hash_table.h"

#define JK_HASH_TABLE_DEFAULT_CAPACITY 16

#define JK_HASH_TABLE_FLAG_FILLED (1 << 0)
#define JK_HASH_TABLE_FLAG_TOMBSTONE (1 << 1)

static JkHashTableSlot *jk_hash_table_probe(JkHashTable *t, JkHashTableKey key)
{
    // Hash and mask off bits to get a result in the range 0..capacity-1. Assumes capacity is a
    // power if 2.
    size_t slot_i = jk_hash_uint32((uint32_t)key) & (t->capacity - 1);
    while ((t->buf[slot_i].flags & JK_HASH_TABLE_FLAG_FILLED) && t->buf[slot_i].key != key) {
        // Linearly probe. Causes more collisions than other methods but seems to make up for it in
        // cache locality.
        slot_i++;
        if (slot_i >= t->capacity) {
            slot_i -= t->capacity;
        }
    }
    return &t->buf[slot_i];
}

static bool jk_hash_table_is_present(JkHashTableSlot *slot)
{
    return (slot->flags & JK_HASH_TABLE_FLAG_FILLED)
            && !(slot->flags & JK_HASH_TABLE_FLAG_TOMBSTONE);
}

static bool jk_is_load_factor_exceeded(size_t count, size_t capacity)
{
    return count > (capacity * JK_HASH_TABLE_LOAD_FACTOR / 10);
}

static bool jk_hash_table_resize(JkHashTable *t)
{
    size_t prev_capacity = t->capacity;
    JkHashTableSlot *prev_buf = t->buf;

    // If the count without tombstones doesn't even exceed the load factor of the previous size,
    // don't increase the capacity. Just redo at the current size to clear out the tombstones.
    if (jk_is_load_factor_exceeded(t->count, t->capacity / 2)) {
        t->capacity *= 2;
    }

    t->buf = calloc(t->capacity, sizeof(JkHashTableSlot));
    if (t->buf == NULL) {
        return false;
    }

    t->tombstone_count = 0;

    for (size_t i = 0; i < prev_capacity; i++) {
        if (jk_hash_table_is_present(&prev_buf[i])) {
            JkHashTableSlot *slot = jk_hash_table_probe(t, prev_buf[i].key);
            slot->key = prev_buf[i].key;
            slot->value = prev_buf[i].value;
            slot->flags |= JK_HASH_TABLE_FLAG_FILLED;
        }
    }
    free(prev_buf);

    return true;
}

static bool jk_is_power_of_two(size_t x)
{
    return (x & (x - 1)) == 0;
}

JkHashTable *jk_hash_table_create_capacity(size_t starting_capacity)
{
    if (!jk_is_power_of_two(starting_capacity)) {
        fprintf(stderr,
                "jk_hash_table_create_capacity: starting_capacity must be a power of two\n");
        exit(1);
    }

    JkHashTable *t = malloc(sizeof(JkHashTable));
    if (!t) {
        return NULL;
    }

    t->buf = calloc(starting_capacity, sizeof(JkHashTableSlot));
    if (!t->buf) {
        return NULL;
    }

    t->capacity = starting_capacity;
    t->count = 0;
    t->tombstone_count = 0;
    return t;
}

JkHashTable *jk_hash_table_create(void)
{
    return jk_hash_table_create_capacity(JK_HASH_TABLE_DEFAULT_CAPACITY);
}

bool jk_hash_table_put(JkHashTable *t, JkHashTableKey key, JkHashTableValue value)
{
    assert(t);

    JkHashTableSlot *slot = jk_hash_table_probe(t, key);

    slot->value = value;

    if (slot->flags
            & JK_HASH_TABLE_FLAG_FILLED) { // If already filled, ensure the tombstone flag is unset
        slot->flags &= ~JK_HASH_TABLE_FLAG_TOMBSTONE;
    } else { // Otherwise, write to key, mark as filled, increase the count, and resize if necessary
        slot->key = key;
        slot->flags |= JK_HASH_TABLE_FLAG_FILLED;
        t->count++;
        if (jk_is_load_factor_exceeded(t->count + t->tombstone_count, t->capacity)) {
            if (!jk_hash_table_resize(t)) {
                return false;
            }
        }
    }

    return true;
}

JkHashTableValue *jk_hash_table_get(JkHashTable *t, JkHashTableKey key)
{
    assert(t);

    JkHashTableSlot *slot = jk_hash_table_probe(t, key);

    if (jk_hash_table_is_present(slot)) {
        return &slot->value;
    } else {
        return NULL;
    }
}

JkHashTableValue *jk_hash_table_get_with_default(
        JkHashTable *t, JkHashTableKey key, JkHashTableValue _default)
{
    assert(t);

    JkHashTableSlot *slot = jk_hash_table_probe(t, key);

    if (slot->flags & JK_HASH_TABLE_FLAG_FILLED) {
        if (slot->flags & JK_HASH_TABLE_FLAG_TOMBSTONE) {
            slot->value = _default;
            slot->flags &= ~JK_HASH_TABLE_FLAG_TOMBSTONE;
        }
    } else {
        slot->key = key;
        slot->value = _default;
        slot->flags |= JK_HASH_TABLE_FLAG_FILLED;
        t->count++;
        if (jk_is_load_factor_exceeded(t->count + t->tombstone_count, t->capacity)) {
            if (jk_hash_table_resize(t)) {
                return jk_hash_table_get(t, key);
            } else {
                return NULL;
            }
        }
    }

    return &slot->value;
}

bool jk_hash_table_remove(JkHashTable *t, JkHashTableKey key)
{
    assert(t);

    JkHashTableSlot *slot = jk_hash_table_probe(t, key);

    if (jk_hash_table_is_present(slot)) {
        slot->flags |= JK_HASH_TABLE_FLAG_TOMBSTONE;
        t->count--;
        t->tombstone_count++;
        return true;
    } else {
        return false;
    }
}

void jk_hash_table_destroy(JkHashTable *t)
{
    assert(t);
    assert(t->buf);

    free(t->buf);
    free(t);
}
