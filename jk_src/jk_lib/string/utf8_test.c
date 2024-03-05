#include <stdio.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/string/utf8.h>
// #jk_build dependencies_end

#define BUFFER_SIZE 256

static void print_unicode(uint32_t codepoint32)
{
    JkUtf8Codepoint codepoint = {0};
    char null_terminated_codepoint[5] = {0};

    jk_utf8_codepoint_encode(codepoint32, &codepoint);
    memcpy(null_terminated_codepoint, codepoint.b, 4);

    printf("U+%04X: %s\n", codepoint32, null_terminated_codepoint);
}

int main(void)
{
    print_unicode(0x0024);
    print_unicode(0x00A3);
    print_unicode(0x0418);
    print_unicode(0x0939);
    print_unicode(0x20AC);
    print_unicode(0xD55C);
    print_unicode(0x10348);

    return 0;
}
