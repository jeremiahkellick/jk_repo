#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "jk_path_utils.h"

static void jk_path_delete_first(char *path)
{
    int length = (int)strlen(path);
    int i;
    for (i = 0; i < length; i++) {
        if (path[i] == '/') {
            break;
        }
    }
    memcpy(path, path + i + 1, length - i);
}

static bool jk_path_is_last_up_dir(char *path)
{
    size_t length = strlen(path);
    return length >= 2 && path[length - 2] == '.' && path[length - 1] == '.';
}

/* PUBLIC */

void jk_delete_last(char *path)
{
    int length = (int)strlen(path);
    if (length > 0) {
        for (int i = length - 1; i >= 0; i--) {
            if (path[i] == '/') {
                path[i] = '\0';
                return;
            }
        }
        path[0] = '\0';
    }
}

/**
 * Converts backslashes to slashes and removes trailing slash
 */
void jk_normalize_path(char *path)
{
    int i;
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
    if (path[i - 1] == '/') {
        path[i - 1] = '\0';
    }
}

/**
 * Fills the buffer with a modified version of the given path which is relative
 * to 'relative_to'
 */
int jk_combine_paths(char *path, char *relative_to, char *buffer, size_t size)
{
    size_t path_length = strlen(path);
    size_t relative_to_length = strlen(relative_to);
    size_t segment_start = relative_to_length + 1;
    size_t cursor = segment_start;

    if (size < path_length + relative_to_length + 2) {
        return JK_COMBINE_PATHS_ERROR_BUFFER_TOO_SMALL;
    }

    memcpy(buffer, relative_to, relative_to_length + 1);

    for (size_t i = 0; i <= path_length; i++) {
        if (path[i] == '/' || i == path_length) {
            buffer[cursor] = '\0';
            if (jk_path_is_last_up_dir(buffer + segment_start) && buffer[0] != '\0'
                    && !jk_path_is_last_up_dir(buffer)) {
                jk_delete_last(buffer);
                cursor = strlen(buffer) + 1;
            } else {
                size_t buffer_length = strlen(buffer);
                if (buffer_length == 0) {
                    memcpy(buffer, buffer + segment_start, cursor - segment_start + 1);
                } else {
                    buffer[buffer_length] = '/';
                    cursor++;
                }
            }
            segment_start = cursor;
        } else {
            buffer[cursor] = path[i];
            cursor++;
        }
    }

    return 0;
}
