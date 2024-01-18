#ifndef JK_PATH_UTILS_H
#define JK_PATH_UTILS_H

void jk_delete_last(char *path);
void jk_normalize_path(char *path);
int jk_combine_paths(char *path, char *relative_to, char *buffer, size_t size);

#define JK_COMBINE_PATHS_ERROR_BUFFER_TOO_SMALL 1

#endif
