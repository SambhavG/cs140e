// low level print that makes a bit easier to debug.
#include "cycle-count.h"
#include "rpi.h"

void notmain(void) {
  unsigned cyc_start = cycle_cnt_read();

  unsigned s = timer_get_usec();
  while ((timer_get_usec() - s) < 1000 * 1000)
    ;

  unsigned cyc_end = cycle_cnt_read();

  printk("cycles per sec = %d\n", cyc_end - cyc_start);
}
