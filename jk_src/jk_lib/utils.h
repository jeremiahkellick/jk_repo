#ifndef JK_UTILS_H
#define JK_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC bool jk_is_power_of_two(size_t x);

#endif
