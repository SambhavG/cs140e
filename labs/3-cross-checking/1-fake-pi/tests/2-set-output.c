#include "test-helper.h"
#include <stdio.h>

void notmain(void) {
#define N 128
  test_gpio_set_output(N);
}
