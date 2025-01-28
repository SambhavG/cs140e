// engler, cs140e: brain-dead generic queue. don't want to use STL/c++ in the kernel.
#ifndef __pQ_H__
#define __pQ_H__
#ifndef E
#error "Client must define the Q datatype <E>"
#endif
#define HEAP_MAXSIZE 1024

typedef struct heapElem {
    E *elem_ptr;
    int priority;
} heapElem_t;

typedef struct pQ {
    unsigned cnt;
    heapElem_t stack[HEAP_MAXSIZE + 1];
} pQ_t;

static unsigned pQ_nelem(pQ_t *q) { return q->cnt; }

static int pQ_empty(pQ_t *q) {
    return (q->cnt == 0);
}

static void pQ_insert(pQ_t *q, E *e, int priority) {
    unsigned bubble_node = q->cnt + 1;
    heapElem_t buffer_node = {.elem_ptr = e, .priority = priority};
    (q->stack)[bubble_node] = buffer_node;
    q->cnt++;

    while (1) {
        // Swap with parent until parent has higher priority
        if (bubble_node > 1 && q->stack[bubble_node].priority > q->stack[bubble_node / 2].priority) {
            heapElem_t swap = q->stack[bubble_node / 2];
            q->stack[bubble_node / 2] = q->stack[bubble_node];
            q->stack[bubble_node] = swap;

            bubble_node /= 2;
        } else {
            break;
        }
    }
}

// Assumes queue is nonempty
static heapElem_t pQ_pop(pQ_t *q) {
    demand(q->cnt, empty heap);
    heapElem_t top_node = q->stack[1];
    unsigned parent = 1;
    // Pull the last node to the top and bubble it down
    q->stack[1] = q->stack[q->cnt];
    q->cnt--;  // Decrease count before bubbling down

    while (1) {
        unsigned child1 = parent * 2;
        unsigned child2 = parent * 2 + 1;
        unsigned largest = parent;

        if (child1 <= q->cnt && q->stack[child1].priority > q->stack[largest].priority) {
            largest = child1;
        }
        if (child2 <= q->cnt && q->stack[child2].priority > q->stack[largest].priority) {
            largest = child2;
        }

        if (largest != parent) {
            heapElem_t swap = q->stack[largest];
            q->stack[largest] = q->stack[parent];
            q->stack[parent] = swap;
            parent = largest;
        } else {
            break;
        }
    }
    return top_node;
}

static heapElem_t pQ_top(pQ_t *q) {
    demand(q->cnt, empty heap);
    return q->stack[1];
}

static void pQ_init(pQ_t *q) {
    q->cnt = 0;
}

static inline pQ_t pQ_mk(void) {
    return (pQ_t){0};
}

#endif
