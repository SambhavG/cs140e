#include "rpi.h"

void notmain(void) {
  const int led = 21;
  const int input = 20;

  gpio_set_output(led);
  gpio_set_input(input);

  while (1) {
    // int signal = gpio_read(input);
    gpio_write(led, gpio_read(input));
    // delay_cycles(10000);
  }
}