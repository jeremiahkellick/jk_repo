#ifndef JK_UTILS_H
#define JK_UTILS_H

#include <stdint.h>

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

uint32_t jk_hash_uint32(uint32_t x);

#endif
