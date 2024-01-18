#ifndef FILE_GET_WORD_COUNT_H
#define FILE_GET_WORD_COUNT_H

#include <stdio.h>

/**
 * Reads file until EOF, returns number of words found
 *
 * @param file The file to read from
 * @return Number of words in the file
 */
int file_get_word_count(FILE *file);

#endif
