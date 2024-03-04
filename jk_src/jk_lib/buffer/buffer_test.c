#include <stdio.h>

#include "buffer.c"

void print_buffer(JkBuffer buffer)
{
    for (size_t i = 0; i < buffer.size; i++) {
        putc(buffer.data[i], stdout);
    }
}

int main(void)
{
    char *null_terminated = "Null terminated string\n";
    JkBuffer buffer = jk_buffer_from_null_terminated(null_terminated);

    print_buffer(JKS("Hello, world!\n"));
    print_buffer(buffer);

    printf("Character in bounds: %d\n", jk_buffer_character_get(buffer, 5));
    printf("Character out of bounds: %d\n", jk_buffer_character_get(buffer, 9001));

    return 0;
}
