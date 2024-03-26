#ifndef JK_PROFILE_H
#define JK_PROFILE_H

#include <stdint.h>

JK_PUBLIC uint64_t jk_os_timer_get(void);

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#define JK_PROFILE_ENTRY_COUNT 0

#define JK_PROFILE_MAX_ENTRIES 256

typedef enum JkProfileEntryType {
    JK_PROFILE_ENTRY_FIRST_CHILD,
    JK_PROFILE_ENTRY_SIBLING,
    JK_PROFILE_ENTRY_PARENT,
} JkProfileEntryType;

typedef struct JkProfileEntry {
    char *name;
    uint64_t start;
    uint64_t elapsed;
    uint64_t elapsed_children;
    uint64_t recursive_calls;
    size_t parent_index;
} JkProfileEntry;

typedef struct JkProfile {
    uint64_t start;
    size_t parent_index;
    JkProfileEntry entries[JK_PROFILE_MAX_ENTRIES];
} JkProfile;

JK_PUBLIC void jk_profile_begin(void);

JK_PUBLIC void jk_profile_end_and_print(void);

JK_PUBLIC size_t jk_profile_time_begin(char *name, size_t entry_index);

JK_PUBLIC void jk_profile_time_end(size_t entry_index);

#define JK_PROFILE_TIME_BEGIN(name) jk_profile_time_begin(name, __COUNTER__ + 1)

#define JK_PROFILE_TIME_END(entry_index) jk_profile_time_end(entry_index)

#endif
