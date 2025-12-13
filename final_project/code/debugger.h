#ifndef __DEBUGGER__
#define __DEBUGGER__

#include "rpi.h"
#include "full-except.h"
#include "mini-watch.h"
#include "mini-step.h"
#include "armv6-debug-impl.h"
#include "memmap.h"
#include <assert.h>
static unsigned hist_n, pc_min, pc_max;
static volatile unsigned *hist;

void gdb_step_handler(void *data, step_fault_t *s);
void gdb_watch_handler(void *data, watch_fault_t *w);

static void gprof_init(void) {
    kmalloc_init(4);
    pc_min = (unsigned)__code_start__;
    pc_max = (unsigned)__code_end__;
    unsigned code_size = (pc_max - pc_min) / 4 + 1;
    assert(pc_min <= pc_max);
    hist_n = (unsigned)kmalloc(code_size);
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
    uint32_t *regs = s->regs->regs;
    unsigned pc = regs[15];
    gprof_inc(pc);
}

static void gdb_init(void) {
    kmalloc_init(4);
    pc_min = (unsigned)__code_start__;
    pc_max = (unsigned)__code_end__;
    unsigned code_size = (pc_max - pc_min) / 4 + 1;
    assert(pc_min <= pc_max);
    mini_bp_init();
    mini_watch_init();
}

enum debugger_action{
    ADD_BP,
    REMOVE_BP,
    STEP,
    CONTINUE,
    EXIT,
    ADD_WP,
    REMOVE_WP,
    WRITE_REG,
    READ_ADDR,
    WRITE_ADDR
};

static int should_disable_stepping = 0;
static int stop_asking_for_input = 0;
static unsigned bp_to_reenable = 0; //If we step when pc is at a breakpoint, we disable it and set this variable
// static unsigned wp_to_reenable = 0; //If we step when pc is at a watchpoint, we disable it and set this variable
// static int hit_wp_fault = 0; //If we hit a watchpoint fault on the previous step

static unsigned gdb_handle_input(char *buf, uint32_t pc) {
    //First character is the action
    enum debugger_action action = buf[0];
    if (action == STEP) {
        if (mini_bp_is_breakpoint((void *)pc)) {
            bp_to_reenable = pc;
            mini_bp_disable((void *)pc);
        }
        return 1;
    }
    if (action == CONTINUE) {
        stop_asking_for_input = 1;
        //If we're on a breakpoint, we need to disable it now and reenable it on the next step
        if (mini_bp_is_breakpoint((void *)pc)) {
            bp_to_reenable = pc;
            mini_bp_disable((void *)pc);
        }
        return 1;
    }
    if (action == ADD_BP) {
        //bytes 1-4 is the address
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        mini_bp_addr((unsigned *)addr, gdb_step_handler, 0);
        return 0;
    }
    if (action == REMOVE_BP) {
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        mini_bp_disable((unsigned *)addr);
        return 0;
    }
    if (action == ADD_WP) {
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        mini_watch_addr((unsigned *)addr, gdb_watch_handler, 0);
        return 0;
    }
    if (action == REMOVE_WP) {
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        mini_watch_disable((unsigned *)addr);
        return 0;
    }
    if (action == READ_ADDR) {
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        delay_ms(5);
        printk("PI: Value at address %x: %x\n", addr, GET32(addr));
        delay_ms(5);
        return 0;
    }
    if (action == WRITE_ADDR) {
        unsigned addr = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        unsigned val = ((int) buf[8] << 24) | ((int) buf[7] << 16) | ((int) buf[6] << 8) | (int) buf[5];
        delay_ms(5);
        printk("PI: Writing value %x to address %x\n", val, addr);
        delay_ms(5);
        PUT32(addr, val);
        return 0;
    }
    if (action == WRITE_REG) {
        unsigned reg = ((int) buf[4] << 24) | ((int) buf[3] << 16) | ((int) buf[2] << 8) | (int) buf[1];
        unsigned val = ((int) buf[8] << 24) | ((int) buf[7] << 16) | ((int) buf[6] << 8) | (int) buf[5];
        delay_ms(5);
        printk("PI: Writing value %x to register %x\n", val, reg);
        delay_ms(5);
    }
    delay_ms(5);
    printk("PI: Received unimplemented command %u\n", action);
    delay_ms(5);
    return 0;
}

void gdb_step_handler(void *data, step_fault_t *s) {
    uint32_t *regs = s->regs->regs;
    unsigned pc = regs[15];
    //Reenable the breakpoint we disabled
    if (bp_to_reenable) {
        mini_bp_addr((unsigned *)bp_to_reenable, gdb_step_handler, 0);
        bp_to_reenable = 0;
    }
    //If we're not on a breakpoint or watchpoint, and we're not asking for input, we can continue
    if (stop_asking_for_input && !mini_bp_is_breakpoint((void *)pc)) {
        return;
    }
    //In a situation where we need input; we're going to re-disable stop_asking_for_input
    //in case the user wants to continue single stepping
    stop_asking_for_input = 0;
    
    //Serialize and send the debugger state
    //Broken up into 64 byte packets, each one labled _D_S_n__ + 56 bytes (14 registers worth) where n is 0,1,...
    char buf0[64] = "_D_S_0__";
    buf0[5] = 0;
    // Copy each register value into the buffer
    for (int i = 0; i < 14; i++) {
        unsigned *reg_ptr = (unsigned *)((char*)buf0 + 8 + i*4);
        // printk("PI: reg %d: %x\n", i, regs[i]);
        *reg_ptr = regs[i]; // Store the register value at current position
    }
    for (int i = 0; i < 64; i++) {
        uart_put8(buf0[i]);
    }
    delay_ms(5);
    char buf1[64] = "_D_S_1__";
    buf1[5] = 1;
    for (int i = 14; i < 17; i++) {
        unsigned *reg_ptr = (unsigned *)((char*)buf1 + 8 + (i-14)*4);
        // printk("PI: reg %d: %x\n", i, regs[i]);
        *reg_ptr = regs[i]; // Store the register value at current position
    }
    for (int i = 0; i < 64; i++) {
        uart_put8(buf1[i]);
    }
    delay_ms(5);
    char buf2[64] = "_D_S_2__";
    buf2[5] = 2;
    //send breakpoints
    for (int i = 0; i < 5; i++) {
        unsigned *bp_ptr = (unsigned *)((char*)buf2 + 8 + i*4);
        if (which_bp_on[i+1]) {
            *bp_ptr = (unsigned)bp_addr_list[i+1];
        } else {
            *bp_ptr = 0;
        }
    }
    for (int i = 0; i < 64; i++) {
        uart_put8(buf2[i]);
    }
    delay_ms(5);
    char buf3[64] = "_D_S_3__";
    buf3[5] = 3;
    //send watchpoints
    for (int i = 0; i < 2; i++) {
        unsigned *wp_ptr = (unsigned *)((char*)buf3 + 8 + i*4);
        if (which_wp_on[i]) {
            *wp_ptr = (unsigned)wp_addr[i];
        } else {
            *wp_ptr = 0xffffffff;
        }
    }
    for (int i = 0; i < 64; i++) {
        uart_put8(buf3[i]);
    }
    delay_ms(5);
    //end transmission
    char buf9[64] = "_D_S_9__";
    buf9[5] = 9;
    for (int i = 0; i < 64; i++) {
        uart_put8(buf9[i]);
    }
    delay_ms(5);
    char read_buf[100];
    while (1) {
        //Read until get a \0
        uint8_t bytes = uart_get8();
        for (int i = 0; i < bytes; i++) {
            read_buf[i] = uart_get8();
        }
        read_buf[bytes] = 0;
        if (gdb_handle_input(read_buf, pc)) {
            break;
        }
    }

}

void gdb_watch_handler(void *data, watch_fault_t *w) {
    delay_ms(5);
    printk("PI: Watchpoint fault at location %x\n", w->fault_addr);
    delay_ms(5);
    printk("PI: Faulting instruction: %x\n", w->fault_pc);
    delay_ms(5);
    step_fault_t s = step_fault_mk(w->fault_pc, w->regs);
    gdb_step_handler(0, &s);
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
