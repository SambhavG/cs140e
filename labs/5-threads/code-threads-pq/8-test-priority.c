#include "test-header.h"

#define int_is_enabled() 0

// number of iterations
static unsigned iter = 0;

void rock_miner(void* rocks) {
    int* rocks_ptr = (int*)rocks;
    unsigned tid = rpi_cur_thread()->tid;
    // trace("Miner with tid=%d, number of rocks=[%x]\n", tid, rocks_remaining);

    while (*rocks_ptr > 0) {
        int priority = rpi_cur_priority();
        trace("[%d priority] I'm the rock miner with tid=%d, with %u rocks remaining. Mining one rock...\n", priority, tid, *rocks_ptr);
        (*rocks_ptr)--;
        iter++;
        rpi_yield();
    }

    rpi_exit(0);
}

void notmain(void) {
    test_init();
    int rocks[10];
    for (int i = 0; i < 10; i++) {
        rocks[i] = i + 1;
        rpi_fork_with_priority(rock_miner, &(rocks[i]), i + 1);
    }
    rpi_thread_start();
    trace("Total rocks mined: %u\n", iter);
}
