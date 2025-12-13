// provides __assert_func for newlib's <assert.h>
#include "rpi.h"

void __assert_func(const char *file, int line, const char *func,
                   const char *expr) {
  printk("ASSERT:%s:%d:%s: `%s` failed\n", file, line, func, expr);
  clean_reboot();
}
