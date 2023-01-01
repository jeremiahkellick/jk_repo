#include <stdarg.h>
#include <stdio.h>

#define limited_put_char(c)                                                    \
    {                                                                          \
        put_char((c), put_char_args);                                          \
        if (++char_count == max_characters) {                                  \
            return char_count;                                                 \
        }                                                                      \
    }

/**
 * String formatter
 *
 * @param put_char Pointer to function to use to output characters
 * @param put_char_args This pointer gets passed along to the put_char function
 *     whenever it's called. Can be used to pass arguments.
 * @param max_characters The maximum number of characters to output. Use a
 *     negative number for unlimited.
 * @param format_string String containing either literal characters to output or
 *     format specifiers used to interpolate in argument values of various types
 * @param ap An argument pointer used to get the values each format specifier
 * @return The number of characters output
 */
int jk_format(void (*put_char)(char c, void *args),
        void *put_char_args,
        int max_characters,
        char *format_string,
        va_list ap)
{
    if (max_characters == 0) {
        return 0;
    }
    int char_count = 0;
    for (char *c = format_string; *c; c++) {
        if (*c != '%') {
            limited_put_char(*c);
            continue;
        }
        char type_signifier = *++c;
        char *string;
        switch (type_signifier) {
        case 's':
            string = va_arg(ap, char *);
            for (; *string; string++) {
                limited_put_char(*string);
            }
            break;
        default:
            limited_put_char('%');
            limited_put_char(type_signifier);
            break;
        }
    }
    return char_count;
}

void file_put_char(char c, void *file)
{
    putc(c, (FILE *)file);
}

void string_put_char(char c, void *pointer)
{
    char **string_pointer = (char **)pointer;
    **string_pointer = c;
    (*string_pointer)++;
}

int jk_fprintf(FILE *file, char *format_string, ...)
{
    va_list ap;
    va_start(ap, format_string);
    int result = jk_format(file_put_char, file, -1, format_string, ap);
    va_end(ap);
    return result;
}

int jk_printf(char *format_string, ...)
{
    va_list ap;
    va_start(ap, format_string);
    int result = jk_format(file_put_char, stdout, -1, format_string, ap);
    va_end(ap);
    return result;
}

int jk_sprintf(char *buffer, int buffer_size, char *format_string, ...)
{
    va_list ap;
    va_start(ap, format_string);
    char *pointer = buffer;
    int num_written = jk_format(
            string_put_char, &pointer, buffer_size - 1, format_string, ap);
    buffer[num_written] = '\0';
    va_end(ap);
    return num_written;
}

int main(void)
{
    jk_printf("%s, %s!\n", "Hello", "mate");
    jk_fprintf(stderr, "%s, %s!\n", "Hello", "bubby");
    char string[16];
    jk_sprintf(string, 16, "%s, %s!\n", "Hello", "gubby");
    fputs(string, stdout);
    char string2[5];
    jk_sprintf(string2, 5, "%s, %s!\n", "Hello", "my guy");
    fputs(string2, stdout);
    putc('\n', stdout);
}
