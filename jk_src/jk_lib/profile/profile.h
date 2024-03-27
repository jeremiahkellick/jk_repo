#ifndef JK_PROFILE_H
#define JK_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

JK_PUBLIC uint64_t jk_os_timer_get(void);

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

typedef struct JkProfileEntry {
    char *name;
    uint64_t start;
    uint64_t elapsed;
    uint64_t elapsed_children;
    uint64_t recursive_calls;
    uint64_t depth;
    struct JkProfileEntry *parent;
    bool appended_to_global_list;
} JkProfileEntry;

typedef struct JkProfile {
    uint64_t start;
    JkProfileEntry *current;
    uint64_t depth;
    size_t entry_count;
    JkProfileEntry *entries[1024];
} JkProfile;

JK_PUBLIC void jk_profile_begin(void);

JK_PUBLIC void jk_profile_end_and_print(void);

JK_PUBLIC void jk_profile_time_begin(JkProfileEntry *entry, char *name);

JK_PUBLIC void jk_profile_time_end();

#define JK_PROFILE_TIME_BEGIN(name)                                \
    do {                                                           \
        static JkProfileEntry jk_profile_time_begin_entry;         \
        jk_profile_time_begin(&jk_profile_time_begin_entry, name); \
    } while (0)

#endif
