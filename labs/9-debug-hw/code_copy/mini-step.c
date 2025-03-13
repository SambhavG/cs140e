// very simple code to just run a single function at user level 
// in mismatch mode.  
//
// search for "todo" and fix.
#include "rpi.h"
#include "armv6-debug-impl.h"
#include "mini-step.h"
#include "full-except.h"

// Define the global variables that were declared as extern in the header
void *bp_addr_list[6] = {0};
int which_bp_on[6] = {0};

//Only the first breakpoint register will be used for single stepping (mismatch)
//The other five are used for regular breakpoints

static step_handler_t step_handler = 0;
static void *step_handler_data = 0;

static step_handler_t bp_handlers[6] = {0};
static void *bp_handler_data[6] = {0};
static int num_bps = 0;

// special function.  never runs, but we know if the traced code
// calls it, that they are done.
void ss_on_exit(int exitcode);

// error checking.
static int single_step_on_p;

// registers where we started single-stepping
static regs_t start_regs;


static int which_bp(void* addr) {
    for (int i = 1; i <= 5; i++) {
        if (which_bp_on[i] && bp_addr_list[i] == addr) {
            return i;
        }
    }
    return -1;
}

static int get_next_available_bp() {
    for (int i = 1; i <= 5; i++) {
        if (!which_bp_on[i]) return i;
    }
    return -1;
}

static int bp_ctrl_is_enabled(unsigned index) {
    assert(index <= 5);
    if (index == 0 && cp14_bcr0_is_enabled()) return 1;
    if (index == 1 && cp14_bcr1_is_enabled()) return 1;
    if (index == 2 && cp14_bcr2_is_enabled()) return 1;
    if (index == 3 && cp14_bcr3_is_enabled()) return 1;
    if (index == 4 && cp14_bcr4_is_enabled()) return 1;
    if (index == 5 && cp14_bcr5_is_enabled()) return 1;
    return 0;
}

static void bp_ctrl_enable(unsigned index) {
    assert(index <= 5);
    if (index == 0) cp14_bcr0_enable();
    if (index == 1) cp14_bcr1_enable();
    if (index == 2) cp14_bcr2_enable();
    if (index == 3) cp14_bcr3_enable();
    if (index == 4) cp14_bcr4_enable();
    if (index == 5) cp14_bcr5_enable();
}

static void bp_ctrl_disable(unsigned index) {
    assert(index <= 5);
    if (index == 0) cp14_bcr0_disable();
    if (index == 1) cp14_bcr1_disable();
    if (index == 2) cp14_bcr2_disable();
    if (index == 3) cp14_bcr3_disable();
    if (index == 4) cp14_bcr4_disable();
    if (index == 5) cp14_bcr5_disable();
}

static unsigned bp_ctrl_get(unsigned index) {
    assert(index <= 5);
    if (index == 0) return cp14_bcr0_get();
    if (index == 1) return cp14_bcr1_get();
    if (index == 2) return cp14_bcr2_get();
    if (index == 3) return cp14_bcr3_get();
    if (index == 4) return cp14_bcr4_get();
    if (index == 5) return cp14_bcr5_get();
    assert(1==0);
}

static void bp_ctrl_set(unsigned index, unsigned val) {
    assert(index <= 5);
    if (index == 0) cp14_bcr0_set(val);
    if (index == 1) cp14_bcr1_set(val); 
    if (index == 2) cp14_bcr2_set(val);
    if (index == 3) cp14_bcr3_set(val);
    if (index == 4) cp14_bcr4_set(val);
    if (index == 5) cp14_bcr5_set(val);
}

static void bp_val_set(unsigned index, void* addr) {
    assert(index <= 5);
    if (index == 0) cp14_bvr0_set((unsigned) addr);
    if (index == 1) cp14_bvr1_set((unsigned) addr);
    if (index == 2) cp14_bvr2_set((unsigned) addr);
    if (index == 3) cp14_bvr3_set((unsigned) addr);
    if (index == 4) cp14_bvr4_set((unsigned) addr);
    if (index == 5) cp14_bvr5_set((unsigned) addr);
}

// 0. get the previous pc that we were mismatching on.
// 1. set bvr0/bcr0 for mismatch on <pc>
// 2. prefetch_flush();
// 3. return old pc.
uint32_t mismatch_pc_set(uint32_t pc) {
    assert(single_step_on_p);
    uint32_t old_pc = cp14_bvr0_get();

    // set a mismatch (vs match) using bvr0 and bcr0 on
    // <pc>
    // todo("setup mismatch on <pc> using bvr0/bcr0");

    //mismatch is setting 10 on bits 22:21
    unsigned b = cp14_bcr0_get();
    b = bits_set(b, 21, 22, 0b10);
    b = bit_clr(b, 20);
    b = bits_set(b, 14, 15, 0b00);
    b = bits_set(b, 5, 8, 0b1111);
    b = bits_set(b, 1, 2, 0b11);
    b = bit_set(b, 0);

    cp14_bcr0_set(b);
    cp14_bvr0_set(pc);
    assert(cp14_bvr0_get() == pc);
    prefetch_flush();

    assert(cp14_bvr0_get() == pc);
    return old_pc;
}

// turn mismatching on: should only be called 1 time.
void mismatch_on(void) {
    single_step_on_p = 1;

    // is ok if we call more than once.
    cp14_enable();

    // we assume they won't jump to 0.
    mismatch_pc_set(0);
}

// disable mis-matching by just turning it off in bcr0
void mismatch_off(void) {
    single_step_on_p = 0;
    // RMW bcr0 to disable breakpoint, 
    // make sure you do a prefetch_flush!
    cp14_bcr0_disable();
    prefetch_flush();
}

// set a mismatch fault on the pc register in <r>
// context switch to <r>
void mismatch_run(regs_t *r) {
    uint32_t pc = r->regs[REGS_PC];
    mismatch_pc_set(pc);
    while(!uart_can_put8());
    switchto(r);
}

void ss_on_exit(int exitcode) {
    switchto(&start_regs);
    // used this when ss couldn't be disabled
    panic("should never reach this!\n");
}



// when we get a mismatch fault.
// 1. check if its for <ss_on_exit>: if so
//    switch back to start_regs.
// 2. otherwise setup the fault and call the
//    handler.  will look like 2-brkpt-test.c
//    somewhat.
// 3. when returns, set the mismatch on the 
//    current pc.
// 4. wait until the UART can get a putc before
//    return (if you don't do this what happens?)
// 5. use switch() to to resume.
static void mismatch_fault(regs_t *r) {
    uint32_t pc = r->regs[REGS_PC];

    // example of using intrinsic built-in routines
    if(pc == (uint32_t)ss_on_exit) {
        output("done pc=%x: resuming initial caller\n", pc);
        switchto(&start_regs);
        not_reached();
    }
    step_fault_t f = step_fault_mk(pc, r);
    step_handler(step_handler_data, &f);
    if (single_step_on_p) {
        mismatch_pc_set(pc);
    }
    while(!uart_can_put8());
    switchto(r);
}

void mini_step_init(step_handler_t h, void *data) {
    assert(h);
    step_handler_data = data;
    step_handler = h; 
    full_except_install(0);
    full_except_set_prefetch(mismatch_fault);
    cp14_enable();

    // just started, should not be enabled.
    assert(!cp14_bcr0_is_enabled());
    assert(!cp14_bcr0_is_enabled());
}

// run <fn> with argument <arg> in single step mode.
uint32_t mini_step_run(void (*fn)(void*), void *arg) {
    uint32_t pc = (uint32_t)fn;
    static void *stack = 0;
    enum { stack_size = 8192*4};
    if(!stack)
        stack = kmalloc(stack_size);
    void *sp = stack + stack_size;
    uint32_t cpsr = cpsr_inherit(USER_MODE, cpsr_get());
    assert(mode_get(cpsr) == USER_MODE);
    regs_t r = (regs_t) {
        .regs[REGS_PC] = (uint32_t)fn,      // where we want to jump to
        .regs[REGS_SP] = (uint32_t)sp,      // the stack pointer to use.
        .regs[REGS_LR] = (uint32_t)ss_on_exit, // where to jump if return.
        .regs[REGS_CPSR] = cpsr             // the cpsr to use.
    };
    mismatch_on();
    switchto_cswitch(&start_regs, &r);
    if (single_step_on_p) {
        mismatch_off();
    }
    return r.regs[0];
}

static int n_faults = 0;
void brkpt_fault(regs_t *r) {
    if(!was_brkpt_fault())
        panic("should only get a breakpoint fault\n");

    // 13-34: effect of exception on watchpoint registers.
    uint32_t pc = r->regs[15];
    output("[brkpt_fault] pc=%x\n", pc);

    //Check which breakpoint is faulting
    int bp_index = which_bp((void*)pc);
    assert(bp_index != -1);
    assert(bp_index != 0); //shouldn't be the mismatch breakpoint
    // printk("[brkpt_fault] bp_index=%d\n", bp_index);

    //Check if the breakpoint is enabled
    assert(bp_ctrl_is_enabled(bp_index));

    // output("[brkpt_fault] ifsr=%x\n", cp15_ifsr_get());
    // output("[brkpt_fault] dscr[2:5]=%b\n", bits_get(cp14_dscr_get(), 2, 5));

    step_fault_t s = step_fault_mk(pc, r);
    bp_handlers[bp_index](bp_handler_data[bp_index], &s);

    n_faults++;
    mini_bp_disable((void*)pc);

    assert(!bp_ctrl_is_enabled(bp_index));
    // switch back and run the faulting instruction.
    switchto(r);
}


void mini_bp_init() {
    full_except_install(0);
    full_except_set_prefetch(brkpt_fault);
    cp14_enable();

    for (int i = 1; i <= 5; i++) {
        assert(!bp_ctrl_is_enabled(i));
    }
}

// set a watch-point on <addr>.
void mini_bp_addr(void *addr, step_handler_t h, void *data) {
    assert(num_bps < 5);

    //Get the next available breakpoint index
    int bp_index = get_next_available_bp();
    which_bp_on[bp_index] = 1;
    bp_addr_list[bp_index] = addr;
    bp_handlers[bp_index] = h;
    bp_handler_data[bp_index] = data;
    num_bps++;
    // see 13-17 for how to set bits
    uint32_t b = bp_ctrl_get(bp_index);

    //clear enable bit and write back, just in case
    bp_ctrl_set(bp_index, bit_clr(b, 0));

    //Write imva to BVR
    bp_val_set(bp_index, addr);

    //Do the stuff listed above - 13-18, 13-19
    b = bits_set(b, 21, 22, 0b00); //no mismatch
    b = bit_clr(b, 20);
    b = bits_set(b, 14, 15, 0b00);
    b = bits_set(b, 5, 8, 0b1111);
    b = bits_set(b, 1, 2, 0b11);
    bp_ctrl_set(bp_index, b);
    bp_ctrl_enable(bp_index);
    // printk("[mini_bp_addr] just enabled breakpoint on addr=%p, bp_index=%d\n", addr, bp_index);
    assert(bp_ctrl_is_enabled(bp_index));
}

// disable current watchpoint <addr>
void mini_bp_disable(void *addr) {
    int bp_index = which_bp(addr);
    assert(bp_index != -1);
    bp_ctrl_disable(bp_index);
    which_bp_on[bp_index] = 0;
    bp_addr_list[bp_index] = 0;
    num_bps--;
}

int mini_bp_is_breakpoint(void *addr) {
    return which_bp(addr) != -1;
}

int mini_bp_enabled() {
    return num_bps;
}

// called from exception handler: if the current 
// fault is a watchpoint, return 1
int mini_bp_is_fault(void) { 
    return was_brkpt_fault();
}

int mini_bp_num_faults() {
    return n_faults;
}
