#include <stdio.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/string/utf8.h>
// #jk_build dependencies_end

#include <windows.h>

#define BUFFER_SIZE 256

static void print_unicode(HANDLE console, uint32_t codepoint32)
{
    JkUtf8Codepoint codepoint;
    char null_terminated_codepoint[5] = {0};
    char buffer[BUFFER_SIZE];

    jk_utf8_codepoint_encode(codepoint32, &codepoint);
    memcpy(null_terminated_codepoint, codepoint.b, 4);

    int len = snprintf(buffer, BUFFER_SIZE, "U+%04X: %s\n", codepoint32, null_terminated_codepoint);

    WriteConsoleA(console, buffer, len, NULL, NULL);
}

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    print_unicode(output, 0x0024);
    print_unicode(output, 0x00A3);
    print_unicode(output, 0x0418);
    print_unicode(output, 0x0939);
    print_unicode(output, 0x20AC);
    print_unicode(output, 0xD55C);
    print_unicode(output, 0x10348);

    return 0;
}
