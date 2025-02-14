#ifndef __DEBUGGER__
#define __DEBUGGER__

#include "rpi.h"
#include "full-except.h"
#include "mini-watch.h"
#include "mini-step.h"
#include "armv6-debug-impl.h"
#include "memmap.h"
static unsigned hist_n, pc_min, pc_max;
static volatile unsigned *hist;

static unsigned gprof_init(void) {
    kmalloc_init();
    pc_min = (unsigned)__code_start__;
    pc_max = (unsigned)__code_end__;
    unsigned code_size = (pc_max - pc_min) / 4 + 1;
    assert(pc_min <= pc_max);
    hist_n = (unsigned)kmalloc(code_size);

    return hist_n;
}

static void gprof_inc(unsigned pc) {
    assert(pc >= pc_min && pc <= pc_max);
    ((unsigned *)hist_n)[(pc - pc_min) / 4]++;
}

static void gprof_dump(unsigned min_val) {
    unsigned length = (pc_max - pc_min) / 4 + 1;
    for (int i = 0; i < length; i++) {
        unsigned *hist = (unsigned *)hist_n;
        unsigned count = hist[i];
        if (count > min_val) {
            printk("%x, %u\n", pc_min + i * 4, count);
        }
    }

}


static void gprof_step_handler(void *data, step_fault_t *s) {
    let regs = s->regs->regs;
    unsigned pc = regs[15];
    gprof_inc(pc);
}

void debugger_init(void) {
    full_except_install(0);
    full_except_set_data_abort(watchpt_fault);
    full_except_set_prefetch(brkpt_fault);
    cp14_enable();
}

static void print_regs(regs_t *r) {
    for (int i = 0; i <= 12; i++) {
        printk("R%d: %x\n", i, r->regs[i]);
    }
    printk("SP: %x\n", r->regs[13]);
    printk("LR: %x\n", r->regs[14]);
    printk("PC: %x\n", r->regs[15]);
    printk("CPSR: %x\n", r->regs[16]);
}

static void sample_bp_handler(void *data, step_fault_t *s) {
    printk("[sample bp handler] Custom bp handler triggered: can see that the breakpoint was triggered at pc=%x\n", s->fault_pc);
}

static void sample_bp_handler_complex(void *data, step_fault_t *s) {
    printk("[sample bp handler] Custom bp handler triggered: can see that the breakpoint was triggered at pc=%x\n", s->fault_pc);
    print_regs(s->regs);
}

static void sample_wp_handler(void *data, watch_fault_t *w) {
    printk("[sample wp handler] Custom wp handler triggered: can see that the watchpoint was triggered at pc=%x\n", w->fault_pc);
}

static void sample_wp_handler_complex(void *data, watch_fault_t *w) {
    printk("[sample wp handler] Custom wp handler triggered: can see that the watchpoint was triggered at pc=%x\n", w->fault_pc);
    print_regs(w->regs);
}

#define bp_addr(addr, handler, data) mini_bp_addr(addr, handler, data)
#define wp_addr(addr, handler, data) mini_watch_addr(addr, handler, data)

#define step_init(handler, data) mini_step_init(handler, data)
#define step_run(fn, data) mini_step_run(fn, data)

#endif
