// handle a store to address 0 (null)
#include "rpi.h"
#include "mini-watch.h"

static volatile uint32_t load_fault_n, store_fault_n;
static volatile uint32_t expected_fault_addr;
static volatile uint32_t expected_fault_pc;

// change to passing in the saved registers.
static void 
watchpt_handler(void *data, watch_fault_t *w) {
    if(w->fault_addr != (void*)expected_fault_addr)
        panic("expected watchpt fault on addr %p, have %p\n",
                        expected_fault_addr, w->fault_addr);
    if(w->fault_pc != expected_fault_pc)
        panic("expected watchpt fault at pc=%p, faulted at=%p\n",
                        expected_fault_pc, w->fault_pc);

    if(w->is_load_p) {
        trace("load fault at pc=%x\n", w->fault_pc);
        assert(w->fault_pc == (uint32_t)GET32);
        load_fault_n++;
    } else {
        trace("store fault at pc=%x\n", w->fault_pc);
        assert(w->fault_pc == (uint32_t)PUT32);
        store_fault_n++;
    }
    mini_watch_disable(w->fault_addr);
}

void notmain(void) {
    mini_watch_init();

    enum { addr1 = 0xaef941ac, addr2 = 0xae1234, val1 = 0xdeadbeef, val2 = 0xeadbeef };

    mini_watch_addr((void*) addr1, watchpt_handler, 0);
    trace("set watchpoint for addr %p\n", addr1);
    mini_watch_addr((void*) addr2, watchpt_handler, 0);
    trace("set watchpoint for addr %p\n", addr2);

    assert(mini_watch_enabled());

    expected_fault_addr = addr1;
    expected_fault_pc = (uint32_t)PUT32;
    trace("should see a store fault!\n");
    PUT32(addr1,val1);
    if(store_fault_n < 1)
        panic("did not see a store fault\n");
    uint32_t got = GET32(addr1);
    if(got != val1)
        panic("expected GET(%x)=%x, have %x\n", addr1, val1, got);

    expected_fault_addr = addr2;
    expected_fault_pc = (uint32_t)PUT32;
    trace("should see a store fault!\n");
    PUT32(addr2,val2);
    if(store_fault_n < 2)
        panic("did not see a store fault\n");
    assert(!mini_watch_enabled());
    got = GET32(addr2);
    if(got != val2)
        panic("expected GET(%x)=%x, have %x\n", addr2, val2, got);
    
    trace("SUCCESS\n");
}
