#include <stdio.h>

#define print_size_in_bits(type) printf(#type "\t%3d\n", (int)sizeof(type) * 8)

typedef struct Struct {
    int size;
    void *buf;
} Struct;

int main(void)
{
    print_size_in_bits(char);
    print_size_in_bits(short);
    print_size_in_bits(int);
    print_size_in_bits(long);
    print_size_in_bits(float);
    print_size_in_bits(double);
    print_size_in_bits(char *);
    print_size_in_bits(size_t);
    print_size_in_bits(Struct);
}
