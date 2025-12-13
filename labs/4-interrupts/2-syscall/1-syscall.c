// do a system call from user level.
//   can check cpsr to see that the
// you have to write:
//  - syscall-asm.S:run_user_code_asm to switch to USER mode.
//  - syscall_vector:(below) to check that it came from USER level.
//  - notmain:(below) to setup stack and install handlers.
//  - _int_table_user[], _int_table_user_end[] tables in interrupt-asm.S
#include "rpi.h"
// asdf
#include "rpi-interrupts.h"
// asdf
#include "rpi-constants.h"

// in syscall-asm.S
void run_user_code_asm(void (*fn)(void), void *stack);

// run <fn> at user level: <stack> must be 8 byte
// aligned
void run_user_code(void (*fn)(void), void *stack) {
  assert(stack);
  demand((unsigned)stack % 8 == 0, stack must be 8 byte aligned);

  // you have to implement <syscall-asm.S:run_user_code_asm>
  // will call <user_fn> with stack pointer <sp>
  run_user_code_asm(fn, stack);
  not_reached();
}

// example of using inline assembly to get the cpsr
// you can also use assembly routines.
static inline uint32_t cpsr_get(void) {
  uint32_t cpsr;
  asm volatile("mrs %0,cpsr" : "=r"(cpsr));
  return cpsr;
}

enum { N = 1024 * 64 };
static uint64_t stack[N];

/*********************************************************************
 * modify below.
 */

// this should be running at user level: you don't have to change this
// routine.
void user_fn(void) {
  uint64_t var;

  trace("checking that stack got switched\n");
  assert(&var >= &stack[0]);
  assert(&var < &stack[N]);

  // use <cpsr_get()> above to get the current mode and check that its
  // at user level.
  unsigned mode = cpsr_get();
  uint32_t mode_mask = (1 << 5) - 1;
  mode = mode & mode_mask;

  if (mode != USER_MODE)
    panic("mode = %b: expected %b\n", mode, USER_MODE);
  else
    trace("cpsr is at user level\n");

  void syscall_hello(void);
  trace("about to call hello\n");
  syscall_hello();

  trace("about to call exit\n");
  void syscall_illegal(void);
  syscall_illegal();

  not_reached();
}

static inline uint32_t spsr_get(void) {
  uint32_t spsr;
  asm volatile("mrs %0,spsr" : "=r"(spsr));
  return spsr;
}

// pc should point to the system call instruction.
//      can see the encoding on a3-29:  lower 24 bits hold the encoding.
int syscall_vector(unsigned pc, uint32_t r0) {
  uint32_t inst, sys_num, mode;

  // make a spsr_get() using cpsr_get() as an example.
  // extract the mode bits and store in <mode>
  // todo("get <spsr> and check that mode bits = USER level\n");

  uint32_t spsr = spsr_get();
  uint32_t mode_mask = (1 << 5) - 1;

  mode = spsr & mode_mask;

  uint32_t mask = (1 << 24) - 1;
  inst = *(uint32_t *)pc;
  sys_num = inst & mask;

  // do not change this code!
  if (mode != USER_MODE)
    panic("mode = %b: expected %b\n", mode, USER_MODE);
  else
    trace("success: spsr is at user level: mode=%b\n", mode);

  // we do a very trivial hello and exit just to show the difference
  switch (sys_num) {
  case 1:
    trace("syscall: hello world\n");
    return 0;
  case 2:
    trace("exiting!\n");
    clean_reboot();
  default:
    printk("illegal system call = %d!\n", sys_num);
    return -1;
  }
}

void notmain() {
  // define a new interrupt vector table, and pass it to
  // rpi-interrupts.h:interrupt_init_v
  // NOTE: make sure you set the stack pointer.
  // todo(
  //     "use rpi-interrupts.h:<interrupt_init_v> (in this dir) "
  //     "to install a interrupt vector with a different swi handler");
  extern uint32_t _new_interrupt_table[];
  extern uint32_t _new_interrupt_table_end[];
  interrupt_init_v(_new_interrupt_table, _new_interrupt_table_end);

  // use the <stack> array above.  note: stack grows down.
  // todo("set <sp> to a reasonable stack address in <stack>");
  uint32_t *sp = (uint32_t *)&stack[N];

  output("calling user_fn with stack=%p\n", sp);
  run_user_code(user_fn, sp);
  not_reached();
}
