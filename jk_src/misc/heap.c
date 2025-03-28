// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

typedef struct Element {
    uint64_t score;
    uint64_t heap_index;
} Element;

typedef struct Heap {
    uint64_t capacity;
    uint64_t count;
    Element **elements;
} Heap;

// Buffer should have at least sizeof(Element) * capcapity bytes
static Heap heap_create(JkBuffer memory)
{
    return (Heap){
        .capacity = memory.size / sizeof(Element),
        .elements = (Element **)memory.data,
    };
}

static uint64_t heap_parent_get(uint64_t i)
{
    return (i - 1) / 2;
}

static void heap_swap(Heap *heap, uint64_t a, uint64_t b)
{
    Element *tmp = heap->elements[a];
    heap->elements[a] = heap->elements[b];
    heap->elements[b] = tmp;
    heap->elements[a]->heap_index = a;
    heap->elements[b]->heap_index = b;
}

static void heapify_up(Heap *heap, uint64_t i)
{
    if (i) {
        uint64_t parent = heap_parent_get(i);
        if (heap->elements[i]->score < heap->elements[parent]->score) {
            heap_swap(heap, i, parent);
            heapify_up(heap, parent);
        }
    }
}

static void heapify_down(Heap *heap, uint64_t i)
{
    uint64_t min_child_score = UINT64_MAX;
    uint64_t min_child = UINT64_MAX;
    for (uint64_t child = 2 * i + 1; child <= 2 * i + 2 && child < heap->count; child++) {
        if (heap->elements[child]->score < min_child_score) {
            min_child_score = heap->elements[child]->score;
            min_child = child;
        }
    }
    if (min_child != UINT64_MAX && heap->elements[min_child]->score < heap->elements[i]->score) {
        heap_swap(heap, i, min_child);
        heapify_down(heap, min_child);
    }
}

static void reheapify(Heap *heap, uint64_t i)
{
    heapify_up(heap, i);
    heapify_down(heap, i);
}

// Returns nonzero on success, zero on failure
static b32 heap_insert(Heap *heap, Element *element)
{
    if (heap->count < heap->capacity) {
        element->heap_index = heap->count++;
        heap->elements[element->heap_index] = element;
        heapify_up(heap, element->heap_index);
        return 1;
    } else {
        return 0;
    }
}

static Element *heap_pop(Heap *heap)
{
    if (heap->count) {
        Element *result = heap->elements[0];
        heap->elements[0] = heap->elements[--heap->count];
        heapify_down(heap, 0);
        return result;
    } else {
        return 0;
    }
}

static Element *heap_peek_min(Heap *heap)
{
    if (heap->count) {
        return heap->elements[0];
    } else {
        return 0;
    }
}

static void heap_verify(Heap *heap)
{
    if (heap->count) {
        for (uint64_t i = heap->count - 1; i > 0; i--) {
            uint64_t parent = heap_parent_get(i);
            JK_ASSERT(heap->elements[parent]->score <= heap->elements[i]->score);
            JK_ASSERT(heap->elements[i]->heap_index == i);
        }
    }
}

int main(void)
{
    Element test_elements[] = {
        {.score = 48},
        {.score = 29},
        {.score = 42},
        {.score = 85},
        {.score = 63},
        {.score = 88},
        {.score = 28},
        {.score = 16},
        {.score = 46},
        {.score = 12},
        {.score = 87},
    };

    Element *buffer[1024];
    Heap heap = heap_create((JkBuffer){.size = sizeof(buffer), .data = (uint8_t *)buffer});

    // Insert first 6 test elements
    for (int i = 0; i < 6; i++) {
        heap_insert(&heap, test_elements + i);
        heap_verify(&heap);
    }

    JK_ASSERT(heap_pop(&heap)->score == 29);
    heap_verify(&heap);
    JK_ASSERT(heap_peek_min(&heap)->score == 42);
    JK_ASSERT(heap_pop(&heap)->score == 42);
    heap_verify(&heap);

    // Insert the rest of the test elements
    for (int i = 6; i < JK_ARRAY_COUNT(test_elements); i++) {
        heap_insert(&heap, test_elements + i);
        heap_verify(&heap);
    }

    JK_ASSERT(heap_peek_min(&heap)->score == 12);

    JK_ASSERT(heap_pop(&heap)->score == 12);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 16);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 28);
    heap_verify(&heap);

    test_elements[4].score = 21;
    reheapify(&heap, test_elements[4].heap_index);
    heap_verify(&heap);

    test_elements[8].score = 86;
    reheapify(&heap, test_elements[8].heap_index);
    heap_verify(&heap);

    JK_ASSERT(heap_pop(&heap)->score == 21);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 48);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 85);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 86);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 87);
    heap_verify(&heap);
    JK_ASSERT(heap_pop(&heap)->score == 88);
    heap_verify(&heap);

    JK_ASSERT(heap.count == 0);
    JK_ASSERT(heap_pop(&heap) == 0);

    JK_ASSERT(heap_peek_min(&heap) == 0);

    return 0;
}
