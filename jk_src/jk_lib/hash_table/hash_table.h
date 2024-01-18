#ifndef JK_HASH_TABLE_H
#define JK_HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Hash table load factor expressed in tenths (e.g. 7 is 7/10 or 70%)
#define JK_HASH_TABLE_LOAD_FACTOR 7

typedef uint32_t JkHashTableKey;
typedef uint32_t JkHashTableValue;

typedef struct JkHashTableSlot {
    uint8_t flags;
    JkHashTableKey key;
    JkHashTableValue value;
} JkHashTableSlot;

typedef struct JkHashTable {
    JkHashTableSlot *buf;
    size_t capacity;
    size_t count;
    size_t tombstone_count;
} JkHashTable;

JkHashTable *jk_hash_table_create(void);

JkHashTable *jk_hash_table_create_capacity(size_t starting_capacity);

bool jk_hash_table_put(JkHashTable *t, JkHashTableKey key, JkHashTableValue value);

JkHashTableValue *jk_hash_table_get(JkHashTable *t, JkHashTableKey key);

JkHashTableValue *jk_hash_table_get_with_default(
        JkHashTable *t, JkHashTableKey key, JkHashTableValue _default);

bool jk_hash_table_remove(JkHashTable *t, JkHashTableKey key);

void jk_hash_table_destroy(JkHashTable *t);

#endif
