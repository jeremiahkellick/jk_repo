#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../jk_lib/jk_path_utils.h"

#define BUFFER_SIZE 512
#define MAX_FILES 128

int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Incorrect number of arguments\n");
    printf("Usage: get_dependencies <file>");
    return 1;
  }

  char *paths[MAX_FILES];
  char paths_buffer[MAX_FILES * BUFFER_SIZE];
  for (int i = 0; i < MAX_FILES; i++) {
    paths[i] = paths_buffer + i * BUFFER_SIZE;
  }
  jk_normalize_path(argv[1]);
  paths[0] = argv[1];
  printf("\"%s\"", paths[0]);

  int current = 0;
  int path_count = 1;

  while (current < path_count) {
    FILE *file;

    if ((file = fopen(paths[current], "r")) == NULL) {
      printf("Could not open file '%s'", paths[current]);
      return 1;
    }

    char relative_to[BUFFER_SIZE];
    memcpy(relative_to, paths[current], strlen(paths[current]));
    jk_delete_last(relative_to);

    bool continueReading = true;

    while (continueReading) {
      char line[BUFFER_SIZE];
      fgets(line, BUFFER_SIZE, file);
      size_t length = strlen(line);
      if (line[0] == '#') {
        if (length > 15 && line[9] == '"' && line[length - 4] == '.' &&
            line[length - 3] == 'h' && line[length - 2] == '"') {
          line[length - 3] = 'c';
          line[length - 2] = '\0';
          if (jk_combine_paths(line + 10, relative_to, paths[path_count],
                               BUFFER_SIZE) == ERROR_BUFFER_TOO_SMALL) {
            printf("Buffer too small");
            return 1;
          };
          bool alreadyExists = false;
          for (int i = 0; i < path_count; i++) {
            if (strncmp(paths[i], paths[path_count], BUFFER_SIZE) == 0) {
              alreadyExists = true;
            }
          }
          if (!alreadyExists) {
            printf(" \"%s\"", paths[path_count]);
            path_count++;
          }
        }
      } else if (line[0] != '\n' && line[0] != '/') {
        continueReading = false;
      }
    }

    fclose(file);
    current++;
  }

  return 0;
}
