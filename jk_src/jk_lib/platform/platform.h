#ifndef JK_PLATFORM_H
#define JK_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// OS functions

JK_PUBLIC size_t jk_platform_file_size(char *file_name);

JK_PUBLIC size_t jk_platform_page_size(void);

JK_PUBLIC void *jk_platform_memory_reserve(size_t size);

JK_PUBLIC bool jk_platform_memory_commit(void *address, size_t size);

JK_PUBLIC void jk_platform_memory_free(void *address, size_t size);

JK_PUBLIC void jk_platform_console_utf8_enable(void);

JK_PUBLIC uint64_t jk_platform_os_timer_get(void);

JK_PUBLIC uint64_t jk_platform_os_timer_frequency_get(void);

// Compiler functions

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void);

#endif
