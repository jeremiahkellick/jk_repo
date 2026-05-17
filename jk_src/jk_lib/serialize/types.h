#ifndef JK_SERIALIZE_TYPES_H
#define JK_SERIALIZE_TYPES_H

#include <jk_src/jk_lib/jk_lib.h>

typedef struct JkSerializer {
    uint64_t version;
    b32 is_writing;
    JkArena *write_arena;
    uint8_t *read_cursor;
} JkSerializer;

#define JK_SERIALIZE_DECL(type) _Static_assert(1, "require semicolon")

#define JK_SERIALIZE_DEF_BEGIN(type) typedef struct type

#define JK_SERIALIZE_DEF_END(type) type

#define JK_SERIALIZE_LEAF(type) _Static_assert(1, "require semicolon")

#define JK_ADD(version_added, type, field) type field

#endif
