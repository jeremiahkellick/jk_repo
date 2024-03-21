#ifndef JK_PROFILE_H
#define JK_PROFILE_H

#include <stdint.h>

JK_PUBLIC uint64_t jk_os_timer_get(void);

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#define JK_PROFILE_MAX_ENTRIES 256

typedef struct JkProfileEntry {
    char *name;
    uint64_t start;
    uint64_t end;
    struct JkProfileEntry *parent;
    struct JkProfileEntry *next_sibling;
    struct JkProfileEntry *first_child;
} JkProfileEntry;

typedef struct JkProfile {
    JkProfileEntry *current;
    size_t count;
    JkProfileEntry entries[JK_PROFILE_MAX_ENTRIES];
} JkProfile;

JK_PUBLIC JkProfileEntry *jk_profile_begin(char *name);

JK_PUBLIC void jk_profile_end(JkProfileEntry *entry);

JK_PUBLIC void jk_profile_print(void);

#endif
