#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_prefetch_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef struct Node {
    struct Node *next;
    uint64_t padding[7];
} Node;

#define NODE_COUNT ((1024llu * 1024 * 1024) / sizeof(Node))

void process_nodes_control(uint64_t rep_count, Node *node);
void process_nodes_prefetch(uint64_t rep_count, Node *node);

typedef struct Function {
    char *name;
    void (*ptr)(uint64_t rep_count, Node *node);
} Function;

Function functions[] = {
    {.name = "Control", .ptr = process_nodes_control},
    {.name = "Prefetch", .ptr = process_nodes_prefetch},
};

// Usage: tests[rep_count_index][function_index]
static JkPlatformRepetitionTest tests[32][JK_ARRAY_COUNT(functions)];

int main(int argc, char **argv)
{
    assert(sizeof(Node) == 64);

    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *memory =
            jk_platform_memory_alloc(NODE_COUNT * sizeof(uint64_t) + NODE_COUNT * sizeof(Node));
    if (!memory) {
        fprintf(stderr, "%s: Could not allocate memory\n", argv[0]);
        exit(1);
    }
    uint64_t *indicies = memory;
    Node *nodes = (Node *)((char *)memory + NODE_COUNT * sizeof(uint64_t));

    srand(1598977331u);

    for (size_t i = 0; i < NODE_COUNT; i++) {
        indicies[i] = i;
    }
    // Shuffle indicies
    for (size_t i = NODE_COUNT; i >= 2; i--) {
        size_t swap_with = rand() % i;
        uint64_t value = indicies[i - 1];
        indicies[i - 1] = indicies[swap_with];
        indicies[swap_with] = value;
    }

    Node *starting_node = &nodes[indicies[0]];

    // Connect nodes into a linked list in random order
    for (size_t i = 0; i < NODE_COUNT; i++) {
        uint64_t idx = indicies[i];
        if (i + 1 < NODE_COUNT) {
            nodes[idx].next = &nodes[indicies[i + 1]];
        }
    }

    for (size_t rep_count_index = 0; rep_count_index < JK_ARRAY_COUNT(tests); rep_count_index++) {
        for (size_t function_index = 0; function_index < JK_ARRAY_COUNT(functions);
                function_index++) {
            Function *function = &functions[function_index];
            JkPlatformRepetitionTest *test = &tests[rep_count_index][function_index];
            uint64_t rep_count = 4 * (rep_count_index + 1);
            if (rep_count_index >= 16) {
                rep_count = 64 * (rep_count_index - 14);
            }

            printf("\nRep count: %llu, Function: %s\n", (long long)rep_count, function->name);

            jk_platform_repetition_test_run_wave(test, NODE_COUNT * sizeof(Node), frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                function->ptr(rep_count, starting_node);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, NODE_COUNT * sizeof(Node));
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    printf("Rep count");
    for (size_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        printf(",%s", functions[i].name);
    }
    printf("\n");
    for (size_t rep_count_index = 0; rep_count_index < JK_ARRAY_COUNT(tests); rep_count_index++) {
        uint64_t rep_count = 4 * (rep_count_index + 1);
        if (rep_count_index >= 16) {
            rep_count = 64 * (rep_count_index - 14);
        }

        printf("%llu", (long long)rep_count);
        for (size_t function_index = 0; function_index < JK_ARRAY_COUNT(functions);
                function_index++) {
            printf(",%.3f",
                    jk_platform_repetition_test_bandwidth(
                            tests[rep_count_index][function_index].min, frequency)
                            / (1024.0 * 1024.0));
        }
        printf("\n");
    }

    return 0;
}
