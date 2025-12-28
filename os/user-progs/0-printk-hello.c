// hard-code print hello in a simple way:
// tests exit and putc.
//
// maybe we should set up our own stack?
#include "libos.h"

void notmain(void) {
  // output("[hello] first hello from pid=$pid\n");

  // for (int i = 0; i < 4; i++) {
  //   int fork_val = sys_fork();
  //   output("[hello] Got a fork value of %d\n", fork_val);
  //   if (fork_val == 0) {
  //     output("[hello] child hello from pid=$pid\n");
  //   } else {
  //     output("[hello] parent hello from pid=$pid\n");
  //   }
  // }
  for (int i = 0; i < 10; i++) {
    int fork_val = sys_fork();
    if (fork_val == 0) {
      continue;
    }
    uint32_t pid = sys_get_pid();

    output("Hello from pid=%d!\n", pid);
    sys_exit(0);
  }

  sys_exit(0);
}
