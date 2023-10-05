#include "hash_table.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_STARTING_CAPACITY 16

#define FLAG_FILLED (1 << 0)
#define FLAG_TOMBSTONE (1 << 1)

/**
 * @brief Returns a hash for the given 32 bit value
 *
 * From https://github.com/skeeto/hash-prospector
 */
static uint32_t hash_uint32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x21f0aaad;
    x ^= x >> 15;
    x *= 0xd35a2d97;
    x ^= x >> 15;
    return x;
}

static JkHashTableSlot *probe(JkHashTable *t, JkHashTableKey key)
{
    // Hash and mask off bits to get a result in the range 0..capacity-1. Assumes capacity is a
    // power if 2.
    size_t slot_i = hash_uint32((uint32_t)key) & (t->capacity - 1);
    while ((t->buf[slot_i].flags & FLAG_FILLED) && t->buf[slot_i].key != key) {
        // Linearly probe. Causes more collisions than other methods but seems to make up for it in
        // cache locality.
        slot_i++;
        if (slot_i >= t->capacity) {
            slot_i -= t->capacity;
        }
    }
    return &t->buf[slot_i];
}

static bool is_present(JkHashTableSlot *slot)
{
    return (slot->flags & FLAG_FILLED) && !(slot->flags & FLAG_TOMBSTONE);
}

static bool is_load_factor_exceeded(size_t count, size_t capacity)
{
    return count > (capacity * JK_HASH_TABLE_LOAD_FACTOR / 100);
}

static bool resize(JkHashTable *t)
{
    size_t prev_capacity = t->capacity;
    JkHashTableSlot *prev_buf = t->buf;

    // If the count without tombstones doesn't even exceed the load factor of the previous size,
    // don't increase the capacity. Just redo at the current size to clear out the tombstones.
    if (is_load_factor_exceeded(t->count, t->capacity / 2)) {
        t->capacity *= 2;
    }

    t->buf = calloc(t->capacity, sizeof(JkHashTableSlot));
    if (t->buf == NULL) {
        return false;
    }

    t->tombstone_count = 0;

    for (size_t i = 0; i < prev_capacity; i++) {
        if (is_present(&prev_buf[i])) {
            JkHashTableSlot *slot = probe(t, prev_buf[i].key);
            slot->key = prev_buf[i].key;
            slot->value = prev_buf[i].value;
            slot->flags |= FLAG_FILLED;
        }
    }
    free(prev_buf);

    return true;
}

static bool is_power_of_two(size_t x)
{
    return (x & (x - 1)) == 0;
}

JkHashTable *jk_hash_table_create_capacity(size_t starting_capacity)
{
    if (!is_power_of_two(starting_capacity)) {
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
    return jk_hash_table_create_capacity(DEFAULT_STARTING_CAPACITY);
}

bool jk_hash_table_put(JkHashTable *t, JkHashTableKey key, JkHashTableValue value)
{
    assert(t);

    JkHashTableSlot *slot = probe(t, key);

    slot->value = value;

    if (slot->flags & FLAG_FILLED) { // If already filled, ensure the tombstone flag is unset
        slot->flags &= ~FLAG_TOMBSTONE;
    } else { // Otherwise, write to key, mark as filled, increase the count, and resize if necessary
        slot->key = key;
        slot->flags |= FLAG_FILLED;
        t->count++;
        if (is_load_factor_exceeded(t->count + t->tombstone_count, t->capacity)) {
            if (!resize(t)) {
                return false;
            }
        }
    }

    return true;
}

JkHashTableValue *jk_hash_table_get(JkHashTable *t, JkHashTableKey key)
{
    assert(t);

    JkHashTableSlot *slot = probe(t, key);

    if (is_present(slot)) {
        return &slot->value;
    } else {
        return NULL;
    }
}

JkHashTableValue *jk_hash_table_get_with_default(
        JkHashTable *t, JkHashTableKey key, JkHashTableValue _default)
{
    assert(t);

    JkHashTableSlot *slot = probe(t, key);

    if (slot->flags & FLAG_FILLED) {
        if (slot->flags & FLAG_TOMBSTONE) {
            slot->value = _default;
            slot->flags &= ~FLAG_TOMBSTONE;
        }
    } else {
        slot->key = key;
        slot->value = _default;
        slot->flags |= FLAG_FILLED;
        t->count++;
        if (is_load_factor_exceeded(t->count + t->tombstone_count, t->capacity)) {
            if (resize(t)) {
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

    JkHashTableSlot *slot = probe(t, key);

    if (is_present(slot)) {
        slot->flags |= FLAG_TOMBSTONE;
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
