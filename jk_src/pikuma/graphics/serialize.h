#include <jk_src/jk_lib/serialize/types.h>

#ifndef JK_SERIALIZE_FUNCTIONS_H

typedef enum Version {
    VER_0,
    VER_FOO,

    VER_COUNT,
} Version;

#endif

JK_SERIALIZE_DEF_BEGIN(State) {
    JK_ADD(VER_FOO, uint64_t, foo);
    JK_ADD(VER_0, uint64_t, flags);
    JK_ADD(VER_0, float, camera_yaw);
    JK_ADD(VER_0, float, camera_pitch);
    JK_ADD(VER_0, JkVec3, player_position);
}
JK_SERIALIZE_DEF_END(State);
