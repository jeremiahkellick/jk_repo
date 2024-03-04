#include <stdio.h>

#define OMIT_NULL_TERMINATOR(string_literal) ((char[sizeof(string_literal) - 1]){string_literal})

int main(void)
{
    printf("sizeof(OMIT_NULL_TERMINATOR(\"12345\")): %zu", sizeof(OMIT_NULL_TERMINATOR("12345")));
    return 0;
}
