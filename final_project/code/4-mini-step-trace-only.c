#include "debugger.h"
#include "rpi.h"

int fib(int n) {
  if (n <= 1)
    return n;
  return fib(n - 1) + fib(n - 2);
}

int fib_caller(void) { return fib(10); }

void notmain(void) {
  gprof_init();
  mini_step_init(gprof_step_handler, 0);

  output("about to run fib()!\n");
  mini_step_run((void *)fib_caller, 0);
  output("done fib()!\n");

  gprof_dump(2);

  clean_reboot();
}
