// test that sw uart works.
#include "rpi.h"
#include "sw-uart.h"
void notmain(void) {
  output("\nTRACE:");
  // hw_uart_disable();
  // use pin 20 for tx, 21 for rx
  sw_uart_t u = sw_uart_init(20, 21, 115200);
  char hello[] = "hello\n\n\n";
  char received[9] = {0};
  // Transmit all characters first
  for (int i = 0; i < 8; i++) {
    received[i] = sw_uart_loopback(&u, hello[i]);
  }
  received[8] = '\0';
  // reset to using the hardware uart.
  // uart_init();
  trace("%s", received);
  trace("if you see `hello` above, sw uart worked!\n");
}