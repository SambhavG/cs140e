// test that sw uart works.
#include "gpio.h"
#include "rpi.h"
#include "sw-uart.h"

void notmain(void) {
  // output("\nTRACE:");
  // hw_uart_disable();

  // use pin 20 for tx, 21 for rx
  sw_uart_t u = sw_uart_init(20, 21, 115200);
  while (1) {
    int received = sw_uart_get8_timeout(&u, 5000000); // 5-second timeout
    if (received == -1) {
      trace("timeout, no data received\n");
    } else {
      trace("Received: %x, %c\n", received, received);
    }
  }
}
