#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "file_get_word_count.h"

// Indicates the word counter is currently outside of a word
#define WORD_OUTSIDE 0
// Indicates the word counter is currently inside of a word
#define WORD_INSIDE 1

int file_get_word_count(FILE *file)
{
    int word_count = 0;
    int state = WORD_OUTSIDE;
    int character;

    while ((character = fgetc(file)) != EOF) {
        int is_character_space = isspace(character);
        switch (state) {
        case WORD_OUTSIDE:
            if (is_character_space) {
                continue;
            }
            state = WORD_INSIDE;
            word_count++;
            break;
        case WORD_INSIDE:
            if (!is_character_space) {
                continue;
            }
            state = WORD_OUTSIDE;
            break;
        default:
            fprintf(stderr, "file_get_word_count: invalid word count state");
            exit(1);
        }
    }

    return word_count;
}
