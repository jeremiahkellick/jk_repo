#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define BUF_SIZE 8

typedef enum Field {
    CHARACTER,
    PINYIN,
    TRANSLATION,
    FIELD_COUNT,
} Field;

typedef struct CharacterData {
    JkBuffer v[FIELD_COUNT];
    bool correct;
    bool written_to_incorrect;
} Character;

static bool is_ignored(uint8_t c)
{
    return c == '\n' || c == '\t';
}

static void print_character(FILE *file, Character *character)
{
    fprintf(file,
            "%.*s\t%.*s\t%.*s\n",
            (int)character->v[CHARACTER].size,
            character->v[CHARACTER].data,
            (int)character->v[PINYIN].size,
            character->v[PINYIN].data,
            (int)character->v[TRANSLATION].size,
            character->v[TRANSLATION].data);
}

static void swap(size_t *a, size_t *b)
{
    size_t tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 text file argument, got %d\n", argv[0], argc - 1);
        exit(1);
    }

    jk_platform_console_utf8_enable();

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (size_t)1 << 35);
    JkBuffer file = jk_platform_file_read_full(&storage, argv[1]);
    size_t file_ptr = 0;

    FILE *incorrect = fopen("incorrect.txt", "wb");
    if (incorrect == NULL) {
        fprintf(stderr, "%s: Could not open 'incorrect.txt': %s\n", argv[0], strerror(errno));
        exit(1);
    }

    Character *characters = jk_arena_pointer_current(&storage);
    size_t character_count = 0;
    while (file_ptr < file.size) {
        // Read character
        while (is_ignored(file.data[file_ptr])) {
            file_ptr++;
        }
        if (file_ptr >= file.size) {
            break;
        }
        character_count++;
        Character *data = jk_arena_push_zero(&storage, sizeof(*data));
        size_t start = file_ptr;
        data->v[CHARACTER].data = file.data + file_ptr;
        do {
            file_ptr++;
        } while (!is_ignored(file.data[file_ptr]));
        data->v[CHARACTER].size = file_ptr - start;

        // Read pinyin
        while (is_ignored(file.data[file_ptr])) {
            file_ptr++;
        }
        if (file_ptr >= file.size) {
            fprintf(stderr, "%s: Unexpected end of file\n", argv[0]);
            exit(1);
        }
        start = file_ptr;
        data->v[PINYIN].data = file.data + file_ptr;
        do {
            file_ptr++;
        } while (!is_ignored(file.data[file_ptr]));
        data->v[PINYIN].size = file_ptr - start;

        // Read translation
        while (is_ignored(file.data[file_ptr])) {
            file_ptr++;
        }
        if (file_ptr >= file.size) {
            fprintf(stderr, "%s: Unexpected end of file\n", argv[0]);
            exit(1);
        }
        start = file_ptr;
        data->v[TRANSLATION].data = file.data + file_ptr;
        do {
            file_ptr++;
        } while (!is_ignored(file.data[file_ptr]));
        data->v[TRANSLATION].size = file_ptr - start;
    }

    int prompt = -1;
    char response[BUF_SIZE] = {'\0'};
    do {
        printf("What would you like to be prompted with?\n"
               "1) Characters\n"
               "2) Pinyin\n"
               "3) Translations\n");
        fgets(response, BUF_SIZE, stdin);
        if (strlen(response) == 2 && response[0] >= '1' && response[0] <= '3') {
            prompt = response[0] - '1';
        } else {
            printf("Invalid response. Try again.\n");
        }
    } while (prompt == -1);

    size_t *indicies = jk_arena_push(&storage, sizeof(*indicies) * character_count);
    for (size_t i = 0; i < character_count; i++) {
        indicies[i] = i;
    }
    srand((unsigned)time(NULL));
    bool all_correct = false;
    while (!all_correct) {
        all_correct = true;
        // Shuffle questions
        for (size_t i = character_count - 1; i > 0; i--) {
            swap(&indicies[i], &indicies[rand() % (i + 1)]);
        }
        for (size_t index = 0; index < character_count; index++) {
            size_t i = indicies[index];
            if (characters[i].correct) {
                continue;
            }

            printf("\n%.*s ", (int)characters[i].v[prompt].size, characters[i].v[prompt].data);

            fgets(response, BUF_SIZE, stdin);

            print_character(stdout, characters + i);

            while (true) {
                printf("Did you get it right (y/n)? ");
                fgets(response, BUF_SIZE, stdin);
                if (strlen(response) == 2 && (response[0] == 'y' || response[0] == 'n')) {
                    characters[i].correct = response[0] == 'y';
                    break;
                } else {
                    printf("Invalid response. Try again.\n");
                }
            }

            if (!characters[i].correct) {
                all_correct = false;
                if (!characters[i].written_to_incorrect) {
                    characters[i].written_to_incorrect = true;
                    print_character(incorrect, characters + i);
                }
            }
        }
    }

    fclose(incorrect);
}
