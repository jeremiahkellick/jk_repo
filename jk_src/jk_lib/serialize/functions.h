#ifndef JK_SERIALIZE_FUNCTIONS_H
#define JK_SERIALIZE_FUNCTIONS_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/serialize/types.h>

#undef JK_SERIALIZE_DECL
#undef JK_SERIALIZE_DEF_BEGIN
#undef JK_SERIALIZE_DEF_END
#undef JK_SERIALIZE_LEAF
#undef JK_ADD

#define JK_SERIALIZE_FUNCTION(type) void serialize_##type(JkSerializer *s, type *d)

#define JK_SERIALIZE_DECL(type) JK_SERIALIZE_FUNCTION(type)

#define JK_SERIALIZE_DEF_BEGIN(type) JK_SERIALIZE_FUNCTION(type)

#define JK_SERIALIZE_DEF_END(type) _Static_assert(1, "require semicolon")

#define JK_SERIALIZE_LEAF(type)                                             \
    JK_SERIALIZE_FUNCTION(type) {                                           \
        if (s->is_writing) {                                                \
            type *pointer = jk_arena_push(s->write_arena, JK_SIZEOF(type)); \
            *pointer = *d;                                                  \
        } else {                                                            \
            *d = *(type *)s->read_cursor;                                   \
            s->read_cursor += JK_SIZEOF(type);                              \
        }                                                                   \
    }                                                                       \
    _Static_assert(1, "require semicolon")

#define JK_ADD(version_added, type, field)                              \
    (version_added <= s->version) ? (serialize_##type(s, &d->field), 1) \
                                  : (jk_memset(&d->field, 0, JK_SIZEOF(d->field)), 0)

JK_SERIALIZE_LEAF(uint64_t);
JK_SERIALIZE_LEAF(float);

JK_SERIALIZE_DEF_BEGIN(JkVec3) {
    JK_ADD(0, float, x);
    JK_ADD(0, float, y);
    JK_ADD(0, float, z);
}
JK_SERIALIZE_DEF_END(JkVec3);

#endif
