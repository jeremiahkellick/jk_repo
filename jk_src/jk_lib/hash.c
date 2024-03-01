#ifndef JK_HASH_C
#define JK_HASH_C

#include "hash.h"

/**
 * @brief Returns a hash for the given 32 bit value
 *
 * From https://github.com/skeeto/hash-prospector
 */
uint32_t jk_hash_uint32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x21f0aaad;
    x ^= x >> 15;
    x *= 0xd35a2d97;
    x ^= x >> 15;
    return x;
}

#endif
