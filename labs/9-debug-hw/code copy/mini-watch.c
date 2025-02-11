// very dumb, simple interface to wrap up watchpoints better.
// only handles a single watchpoint.
//
// You should be able to take most of the code from the 
// <1-watchpt-test.c> test case where you already did 
// all the needed operations.  This interface just packages 
// them up a bit.
//
// possible extensions:
//   - do as many as hardware supports, perhaps a handler for 
//     each one.
//   - make fast.
//   - support multiple independent watchpoints so you'd return
//     some kind of structure and pass it in as a parameter to
//     the routines.
#include "rpi.h"
#include "mini-watch.h"
#include "full-except.h"

// we have a single handler: so just use globals.
// static watch_handler_t watchpt_handler = 0;
// static void *watchpt_data = 0;
// static void *watchpoint_addr;

static watch_handler_t watchpt_handler[2];
static void *watchpt_data[2];
static void *watchpoint_addr[2];
static int num_watchpoints = 0;


//Tells you which watchpoint is the one looking at this address
static int which_wp(void* addr) {
    for (int i = 0; i < num_watchpoints; i++) {
        if (watchpoint_addr[i] == addr) {
            return i;
        }
    }
    return -1;
}

static int wp_ctrl_is_enabled(unsigned index) {
    if (index == 0 && cp14_wcr0_is_enabled()) return 1;
    if (index == 1 && cp14_wcr1_is_enabled()) return 1;
    return 0;
}

static int wp_ctrl_enable(unsigned index) {
    if (index == 0) cp14_wcr0_enable();
    if (index == 1) cp14_wcr1_enable();
}

static int wp_ctrl_disable(unsigned index) {
    if (index == 0) cp14_wcr0_disable();
    if (index == 1) cp14_wcr1_disable();
}

static unsigned wp_ctrl_get(unsigned index) {
    if (index == 0) return cp14_wcr0_get();
    if (index == 1) return cp14_wcr1_get(); 
}

static unsigned wp_ctrl_set(unsigned index, unsigned val) {
    if (index == 0) cp14_wcr0_set(val);
    if (index == 1) cp14_wcr1_set(val); 
}

static int wp_val_set(unsigned index, void* addr) {
    if (index == 0) cp14_wvr0_set(addr);
    if (index == 1) cp14_wvr1_set(addr);
}

// is it a load fault?
static int mini_watch_load_fault(void) {
    return datafault_from_ld();
}

// if we have a dataabort fault, call the watchpoint
// handler.
static void watchpt_fault(regs_t *r) {
    // watchpt handler.
    if(was_brkpt_fault())
        panic("should only get debug faults!\n");
    if(!was_watchpt_fault())
        panic("should only get watchpoint faults!\n");
    if(!watchpt_handler)
        panic("watchpoint fault without a fault handler\n");

    watch_fault_t w = {0};

    w.fault_pc = watchpt_fault_pc();
    w.is_load_p = mini_watch_load_fault();
    w.regs = r;
    void* fault_addr = (void*) cp15_far_get();
    w.fault_addr = fault_addr;

    watchpt_handler(watchpt_data, &w);

    //Disable whichever watchpoint was triggered
    int triggered_watchpoint = which_wp(fault_addr);

    wp_ctrl_disable(triggered_watchpoint);

    assert(!wp_ctrl_disable(triggered_watchpoint));

    // in case they change the regs.
    switchto(w.regs);
}

// setup:
//   - exception handlers, 
//   - cp14, 
//   - setup the watchpoint handler
// (see: <1-watchpt-test.c>
void mini_watch_init(watch_handler_t h, void *data) {
    // todo("setup cp14 and <watchpt_fault> using full_except");
    full_except_install(0);
    full_except_set_data_abort(watchpt_fault);
    cp14_enable();
    uint32_t b = 0;
    b = bit_clr(b, 20); //disable linking
    b = bits_clr(b, 14, 15); //watchpoint matches in secure or non-secure world
    b  = bits_set(b, 5, 8, 0b0000); //watchpoint is triggered for any access 0x0,1,2,3
    b = bits_set(b, 3, 4, 0b11); //for both loads and stores
    b = bits_set(b, 1, 2, 0b11); //for both user and privileged
    // b = bit_set(b, 0); //enable watchpoint
    cp14_wcr0_set(b);

    // just started, should not be enabled.
    assert(!cp14_bcr0_is_enabled());
    assert(!cp14_bcr0_is_enabled());

    watchpt_handler = h;
    watchpt_data = data;
}

// set a watch-point on <addr>.
void mini_watch_addr(void *addr) {
    assert(num_watchpoints < 2);
    wp_val_set(num_watchpoints, addr);

    uint32_t bottom_bits = (unsigned) addr & 0b11;

    uint32_t b = wp_ctrl_get(num_watchpoints);
    b = bits_set(b, 5, 8, 0b0001 << bottom_bits);
    b = bit_set(b, 0);
    wp_ctrl_set(num_watchpoints, b);

    assert(wp_ctrl_is_enabled(num_watchpoints));
    prefetch_flush();
    watchpoint_addr[num_watchpoints] = addr;

    num_watchpoints++;
}

// disable current watchpoint <addr>
void mini_watch_disable(void *addr) {
    cp14_wcr0_disable();
}

// return 1 if enabled.
int mini_watch_enabled(void) {
    return cp14_wcr0_is_enabled();
}

// called from exception handler: if the current 
// fault is a watchpoint, return 1
int mini_watch_is_fault(void) { 
    return was_watchpt_fault();
}
