void jk_delete_last(char *path);
void jk_normalize_path(char *path);
int jk_combine_paths(char *path, char *relative_to, char *buffer, size_t size);

#define ERROR_BUFFER_TOO_SMALL 1
