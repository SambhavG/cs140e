// simple breakpoint test:
//  1. set a single breakpoint on <foo>.
//  2. in exception handler, make sure the fault pc = <foo> and disable
//     the breakpoint.
//  3. if that worked, do 1&2 <n> times to make sure works.
#include "armv6-debug-impl.h"
#include "full-except.h"
#include "mini-step.h"
#include "rpi.h"
#include "vector-base.h"

static void bp_handler(void *data, step_fault_t *s) {
  printk("[Custom handler] Custom bp handler triggered: can see that the "
         "breakpoint was triggered at pc=%x\n",
         s->fault_pc);
  printk("[Custom handler] The registers are: %x, %x, %x, %x, %x\n",
         s->regs->regs[0], s->regs->regs[1], s->regs->regs[2], s->regs->regs[3],
         s->regs->regs[4]);
}

// the routine we fault on: we don't want to use GET32 or PUT32
// since that would break printing.
void foo1(int x);
void foo2(int x);
void foo3(int x);
void foo4(int x);
void foo5(int x);

void notmain(void) {
  mini_bp_init();

  mini_bp_addr(foo1, bp_handler, NULL);

  output("set breakpoint for addr %p\n", foo1);

  output("about to call %p: should see a fault!\n", foo1);
  foo1(0);
  assert(mini_bp_num_faults() == 1);

  int n = 10;
  trace("worked!  fill do %d repeats\n", n);
  for (int i = 0; i < n; i++) {
    mini_bp_addr(foo1, bp_handler, NULL);
    mini_bp_addr(foo2, bp_handler, NULL);
    mini_bp_addr(foo3, bp_handler, NULL);
    mini_bp_addr(foo4, bp_handler, NULL);
    mini_bp_addr(foo5, bp_handler, NULL);
    trace("should see five breakpoint faults!\n");
    foo1(i);
    foo2(i);
    foo3(i);
    foo4(i);
    foo5(i);
    trace("n_faults=%d, i=%d\n", mini_bp_num_faults(), i);
    assert(mini_bp_num_faults() == 6 + i * 5);
  }
  trace("SUCCESS\n");
}

// weak attempt at preventing inlining.
void foo1(int x) { trace("running foo1: %d\n", x); }
void foo2(int x) { trace("running foo2: %d\n", x); }
void foo3(int x) { trace("running foo3: %d\n", x); }
void foo4(int x) { trace("running foo4: %d\n", x); }
void foo5(int x) { trace("running foo5: %d\n", x); }