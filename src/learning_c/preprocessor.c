#include <stdio.h>

#define swap(t, a, b) \
    {                 \
        t = a;        \
        a = b;        \
        b = t;        \
    }

#define dbg_print_string(str) printf(#str ": \"%s\"\n", str)
#define dbg_print_int(i) printf(#i ": %d\n", i)

// ---- Define OS_NAME ---------------------------------------------------------
#ifdef _WIN32
    #define OS_NAME "Windows"
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define OS_NAME "macOS"
    #else
        #define OS_NAME "Unknown"
    #endif
#elif __linux__
    #define OS_NAME "Linux"
#else
    #define OS_NAME "Unknown"
#endif
// -----------------------------------------------------------------------------

#define print_os() printf("OS: " OS_NAME "\n")

int main(void)
{
    char *tmp_s;
    char *a = "a";
    char *b = "b";

    int tmp_i;
    int one = 1;
    int two = 2;

    print_os();

    swap(tmp_s, a, b);
    dbg_print_string(a);
    dbg_print_string(b);

    swap(tmp_i, one, two);
    dbg_print_int(one);
    dbg_print_int(two);

    return 0;
}
