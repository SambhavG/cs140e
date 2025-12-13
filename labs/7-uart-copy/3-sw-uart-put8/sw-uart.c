#include "sw-uart.h"
#include "cycle-count.h"
#include "cycle-util.h"
#include "rpi.h"

#include <stdarg.h>

// bit bang the 8-bits <b> to the sw-uart <uart>.
//  - at 115200 baud you can probably just use microseconds,
//    but at faster rates you'd want to use cycles.
//  - libpi/include/cycle-util.h has some helper
//    that you can use if you want (don't have to).
//
// recall:
//    - the microseconds to write each bit (0 or 1) is in
//      <uart->usec_per_bit>
//    - the cycles to write each bit (0 or 1) is in
//      <uart->cycle_per_bit>
//    - <cycle_cnt_read()> counts cycles
//    - <timer_get_usec()> counts microseconds.
void sw_uart_put8(sw_uart_t *uart, uint8_t b) {
  // use local variables to minimize any loads or stores
  int tx = uart->tx;
  uint32_t n = uart->cycle_per_bit, u = n, s = cycle_cnt_read();
  // start bit
  write_cyc_until(tx, 0, s, u);
  u += n;
  for (int i = 0; i < 8; i++) {
    write_cyc_until(tx, b & (0b1 << i), s, u);
    u += n;
  }
  // stop bit
  write_cyc_until(tx, 1, s, u);
}

// optional: do receive.
//      EASY BUG: if you are reading input, but you do not get here in
//      time it will disappear.
int sw_uart_get8_timeout(sw_uart_t *uart, uint32_t timeout_usec) {
  unsigned rx = uart->rx;

  // get the start bit:
  // right away (used to be return never).
  if (gpio_read(rx) != 0b1) {
    printk("SAW bad bit at beginning\n");
    return -1;
  }
  // printk("SAW idle bit %x\n", gpio_read(rx));
  if (!wait_until_usec(rx, 0, timeout_usec)) {
    return -1;
  }
  // printk("SAW start bit %x\n", gpio_read(rx));

  unsigned current_time = cycle_cnt_read();
  unsigned cycle_time = uart->cycle_per_bit;
  unsigned message = 0;

  // strategy: read halfway in between each cycle, after last bit has been
  // written but before the next bit has been written
  for (int i = 0; i < 8; i++) {
    delay_ncycles(current_time, (i + 1) * cycle_time + cycle_time / 2);
    message |= (GPIO_READ_RAW(rx) << i);
    // printk("SAW bit %d as %x", i,);
  }
  // get the stop bit: should be coming at the next cycle
  if (!wait_until_cyc(rx, 1, current_time, 9 * cycle_time + cycle_time / 2)) {
    printk("OOOPS SAW BAD STOP BIT\n");
    return -1;
  }
  // printk("SAW stop bit %x\n", gpio_read(rx));
  return message;
}

uint8_t sw_uart_loopback(sw_uart_t *uart, uint8_t b) {
  // use local variables to minimize any loads or stores
  unsigned tx = uart->tx;
  unsigned rx = uart->rx;
  unsigned message = 0;

  uint32_t n = uart->cycle_per_bit, u = n, s = cycle_cnt_read();
  // start bit
  write_cyc_until(tx, 0, s, u);
  u += n;
  for (int i = 0; i < 8; i++) {
    write_cyc_until(tx, b & (0b1 << i), s, u);

    delay_ncycles(s, u - (n / 2));
    message |= (GPIO_READ_RAW(rx) << i);

    u += n;
  }
  // stop bit
  write_cyc_until(tx, 1, s, u);
  return message;
}

int sw_uart_raj_was_here(void) { return -1; }

// finish implementing this routine.
sw_uart_t sw_uart_init_helper(unsigned tx, unsigned rx, unsigned baud,
                              unsigned cyc_per_bit, unsigned usec_per_bit) {
  // remedial sanity checking
  assert(tx && tx < 31);
  assert(rx && rx < 31);
  assert(cyc_per_bit && cyc_per_bit > usec_per_bit);
  assert(usec_per_bit);

  // basic sanity checking.  if this fails lmk
  unsigned mhz = 700 * 1000 * 1000;
  unsigned derived = cyc_per_bit * baud;
  if (!((mhz - baud) <= derived && derived <= (mhz + baud)))
    panic("too much diff: cyc_per_bit = %d * baud = %d\n", cyc_per_bit,
          cyc_per_bit * baud);

  // make sure you set TX to its correct default!
  gpio_set_input(rx);
  gpio_set_pullup(rx);
  gpio_set_output(tx);
  gpio_set_on(tx);
  // todo("setup rx,tx and initial state of tx pin.");

  return (sw_uart_t){.tx = tx,
                     .rx = rx,
                     .baud = baud,
                     .cycle_per_bit = cyc_per_bit,
                     .usec_per_bit = usec_per_bit};
}
