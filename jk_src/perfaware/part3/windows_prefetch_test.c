#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_prefetch_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

typedef struct Node {
    struct Node *next;
    uint32_t num;
} Node;

#define NODE_COUNT ((1024llu * 1024 * 1024) / sizeof(Node))

void process_nodes_control(Node *node);
void process_nodes_prefetch(Node *node);

typedef struct Function {
    char *name;
    void (*ptr)(Node *node);
} Function;

Function functions[] = {
    {.name = "Control", .ptr = process_nodes_control},
    {.name = "Prefetch", .ptr = process_nodes_prefetch},
};

// Usage: tests[0 - don't prefetch, 1 - prefetch]
static JkRepetitionTest tests[JK_ARRAY_COUNT(functions)];

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);

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
        nodes[idx].num = (uint32_t)i;
    }

    while (true) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
            Function *function = &functions[i];
            JkRepetitionTest *test = &tests[i];

            printf("\n%s\n", function->name);

            jk_repetition_test_run_wave(test, NODE_COUNT * sizeof(Node), frequency, 10);
            while (jk_repetition_test_running(test)) {
                jk_repetition_test_time_begin(test);
                function->ptr(starting_node);
                jk_repetition_test_time_end(test);
                jk_repetition_test_count_bytes(test, NODE_COUNT * sizeof(Node));
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
