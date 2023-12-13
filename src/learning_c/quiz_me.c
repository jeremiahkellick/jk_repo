/**
 * @file Program to quiz yourself using questions in a text file
 *
 * This program prompts you with a random question and reveals the answer when you press Enter. Then
 * it asks you if you got the question right. If you answer 'n', it asks you the question again
 * later.
 *
 * Usage: quiz_me FILE
 *
 * FILE should be a text file formatted as follows
 *
 * question 1
 * answer 1
 *
 * question 2
 * answer 2
 *
 * etc
 *
 * Each question and answer pair should be exactly two lines. You can't split up a question or an
 * answer into multiple lines.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 1024
#define MAX_QUESTIONS 1024

typedef struct Question {
    char *question;
    char *answer;
} Question;

Question questions[MAX_QUESTIONS];

int jk_read_line(FILE *file, char *buf, size_t buf_size, bool *eof)
{
    int i;
    for (i = 0; i < buf_size; i++) {
        int c = fgetc(file);
        char d = (char)c;
        if (c == '\n') {
            break;
        }
        if (c == EOF) {
            *eof = true;
            break;
        }
        buf[i] = (char)c;
    }
    return i;
}

char *read_line_malloc(FILE *file, char *tmp_buf, size_t tmp_buf_size)
{
    bool eof = false;
    int length = jk_read_line(file, tmp_buf, tmp_buf_size, &eof);
    if (length == 0) {
        if (eof) {
            return NULL;
        } else { // We want to skip empty lines, so just read again
            return read_line_malloc(file, tmp_buf, tmp_buf_size);
        }
    }
    char *new_buf = malloc(sizeof(char) * (length + 1 /* Add 1 for null terminator */));
    memcpy(new_buf, tmp_buf, length);
    new_buf[length] = '\0';
    return new_buf;
}

void empty_stdin(void)
{
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}

void swap(Question *a, Question *b)
{
    Question tmp = *b;
    *b = *a;
    *a = tmp;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("%s: Incorrect number of arguments. Usage: quiz_me FILE", argv[0]);
        exit(1);
    }

    // Read questions from file
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror(argv[0]);
        exit(1);
    }
    char buf[BUF_SIZE];
    int num_questions = -1;
    while (true) {
        num_questions++;
        if (num_questions >= MAX_QUESTIONS) {
            printf("%s: MAX_QUESTIONS exceeded", argv[0]);
            fclose(file);
            exit(1);
        }
        questions[num_questions].question = read_line_malloc(file, buf, BUF_SIZE);
        if (questions[num_questions].question == NULL) {
            break;
        }
        questions[num_questions].answer = read_line_malloc(file, buf, BUF_SIZE);
        if (questions[num_questions].answer == NULL) {
            break;
        }
    }
    fclose(file);

    // Questions with indexes >= this were answered correctly. We're done when this reachers zero.
    int correct = num_questions;
    srand(time(NULL));
    while (correct > 0) {
        // Shuffle questions
        for (int i = correct - 1; i > 0; i--) {
            swap(&questions[i], &questions[rand() % (i + 1)]);
        }

        // Ask questions
        int i = 0;
        while (i < correct) {
            printf("\n%s ", questions[i].question);
            empty_stdin();
            printf("%s\n", questions[i].answer);
            printf("Did you get it right (y/n)? ");
            int c = getchar();
            empty_stdin();
            if (tolower(c) == 'y') {
                // Move question to correct
                swap(&questions[i], &questions[--correct]);
            } else {
                i++;
            }
        }
    }

    printf("\nAll questions answered correctly!");
    return 0;
}
