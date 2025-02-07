#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/learning_c/word_count/file_get_word_count.h>
// #jk_build dependencies_end

#define TMP_FILE_NAME "tmp_file_get_word_count_test.txt"
#define EXPECTED_COUNT 69

const char *const lorem_ipsum =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
        "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
        "aliquip ex ea commodo consequat. Duis aute irure dolor in "
        "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
        "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
        "culpa qui officia deserunt mollit anim id est laborum.";

int main(void)
{
    printf("file_get_word_count()\n    ");

    // Write lorem_ipsum to a temporary file
    FILE *tmp_file_w = fopen(TMP_FILE_NAME, "w");
    if (!tmp_file_w) {
        printf("FAIL: test failed to open temporary file for writing\n");
        return 1;
    }
    fprintf(tmp_file_w, "%s\n", lorem_ipsum);
    fclose(tmp_file_w);

    // Open temporary file for reading
    FILE *tmp_file_r = fopen(TMP_FILE_NAME, "r");
    if (!tmp_file_r) {
        printf("FAIL: test failed to open temporary file for reading\n");
        return 1;
    }

    // Read file and clean up
    int actual_count = file_get_word_count(tmp_file_r);
    fclose(tmp_file_r);
    remove(TMP_FILE_NAME);

    // Check result
    if (actual_count != EXPECTED_COUNT) {
        printf("FAIL: expected %d, got %d\n", EXPECTED_COUNT, actual_count);
        return 1;
    }
    printf("SUCCESS\n");
    return 0;
}
