#include "debugger.h"

// static void bp_handler(void *data, step_fault_t *s) {
//     printk("[Custom bp handler] Custom bp handler triggered: can see that the
//     breakpoint was triggered at pc=%x\n", s->fault_pc); printk("[Custom bp
//     handler] The registers are: %x, %x, %x, %x, %x\n", s->regs->regs[0],
//     s->regs->regs[1], s->regs->regs[2], s->regs->regs[3], s->regs->regs[4]);
// }

// static void wp_handler(void *data, watch_fault_t *w) {
//     printk("[Custom wp handler] Custom wp handler triggered: can see that the
//     watchpoint was triggered at pc=%x\n", w->fault_pc); printk("[Custom wp
//     handler] The registers are: %x, %x, %x, %x, %x\n", w->regs->regs[0],
//     w->regs->regs[1], w->regs->regs[2], w->regs->regs[3], w->regs->regs[4]);
// }

void foo1(int x) { trace("running foo1: %d\n", x); }
void foo2(int x) { trace("running foo2: %d\n", x); }
void foo3(int x) { trace("running foo3: %d\n", x); }
void foo4(int x) { trace("running foo4: %d\n", x); }
void foo5(int x) { trace("running foo5: %d\n", x); }

int fib(int n) {
  if (n <= 1)
    return n;
  GET32(0xdeadbee0);
  return fib(n - 1) + fib(n - 2);
}

int fib_caller(void) { return fib(10); }

void notmain(void) {
  debugger_init();

  // int n = 10;
  // for(int i = 0; i < n; i++) {
  //     bp_addr(foo1, sample_bp_handler, NULL);
  //     bp_addr(foo2, sample_bp_handler, NULL);
  //     bp_addr(foo3, sample_bp_handler, NULL);
  //     bp_addr(foo4, sample_bp_handler, NULL);
  //     bp_addr(foo5, sample_bp_handler_complex, NULL);
  //     wp_addr((void*)0xdeadbeef, sample_wp_handler, NULL);
  //     wp_addr((void*)0xceadbeef, sample_wp_handler_complex, NULL);
  //     foo1(i);
  //     foo2(i);
  //     foo3(i);
  //     foo4(i);
  //     foo5(i);
  //     PUT32(0xdeadbeef, i);
  //     PUT32(0xceadbeef, i);
  //     trace("n_faults=%d, i=%d\n\n\n", mini_bp_num_faults(),i);
  //     assert(mini_bp_num_faults() == 5+i*5);
  // }

  // Profile
  gdb_init();
  step_init(gdb_step_handler, 0);

  output("about to run fib()!\n");
  step_run((void *)fib_caller, 0);
  output("done fib()!\n");

  // gprof_dump(2);

  clean_reboot();

  trace("SUCCESS\n");
}