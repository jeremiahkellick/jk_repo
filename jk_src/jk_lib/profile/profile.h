#ifndef JK_PROFILE_H
#define JK_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

JK_PUBLIC uint64_t jk_os_timer_get(void);

JK_PUBLIC uint64_t jk_os_timer_frequency_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_get(void);

JK_PUBLIC uint64_t jk_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#ifndef JK_PROFILE_DISABLE
#define JK_PROFILE_DISABLE 0
#endif

#if JK_PROFILE_DISABLE

#define JK_PROFILE_TIME_BEGIN(...)
#define JK_PROFILE_TIME_END(...)

#else

typedef struct JkProfileEntry {
    char *name;
    uint64_t elapsed_exclusive;
    uint64_t elapsed_inclusive;
    uint64_t hit_count;
    uint64_t depth;

#ifndef NDEBUG
    int64_t active_count;
#endif

    bool seen;
} JkProfileEntry;

typedef struct JkProfileTiming {
    uint64_t saved_elapsed_inclusive;
    JkProfileEntry *parent;
    uint64_t start;

#ifndef NDEBUG
    JkProfileEntry *entry;
    bool ended;
#endif
} JkProfileTiming;

JK_PUBLIC void jk_profile_time_begin(JkProfileTiming *timing, JkProfileEntry *entry, char *name);

JK_PUBLIC void jk_profile_time_end(JkProfileTiming *timing);

#define JK_PROFILE_TIME_BEGIN(identifier)                                                     \
    JkProfileTiming jk_profile_timing__##identifier;                                          \
    do {                                                                                      \
        static JkProfileEntry jk_profile_time_begin_entry;                                    \
        jk_profile_time_begin(                                                                \
                &jk_profile_timing__##identifier, &jk_profile_time_begin_entry, #identifier); \
    } while (0)

#define JK_PROFILE_TIME_END(identifier) jk_profile_time_end(&jk_profile_timing__##identifier);

#endif

JK_PUBLIC void jk_profile_begin(void);

JK_PUBLIC void jk_profile_end_and_print(void);

#endif
