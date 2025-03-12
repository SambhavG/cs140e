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
    let regs = s->regs->regs;
    unsigned pc = regs[15];
    gprof_inc(pc);
}

static void gdb_init(void) {
    kmalloc_init(4);
    pc_min = (unsigned)__code_start__;
    pc_max = (unsigned)__code_end__;
    unsigned code_size = (pc_max - pc_min) / 4 + 1;
    assert(pc_min <= pc_max);
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

static unsigned gdb_handle_input(char *buf) {
    //First character is the action
    enum debugger_action action = buf[0];
    if (action == STEP) {
        // delay_ms(10);
        // printk("PI: Received STEP command\n");
        // delay_ms(10);
        return 1;
    }
    if (action == CONTINUE) {
        mismatch_off();

        // should_disable_stepping = 1;
        return 1;
    }
    delay_ms(10);
    printk("PI: Received unimplemented command %u\n", action);
    delay_ms(10);
    return 0;
}

static enum step_handler_res gdb_step_handler(void *data, step_fault_t *s) {
    uint32_t *regs = s->regs->regs;
    unsigned pc = regs[15];
    
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
    //end transmission
    char buf2[64] = "_D_S_9__";
    buf2[5] = 9;
    for (int i = 0; i < 64; i++) {
        uart_put8(buf2[i]);
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
        if (gdb_handle_input(read_buf)) {
            break;
        }
    }
    
    return 1;
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
