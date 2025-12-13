// engler, cs140 put your gpio-int implementations in here.
#include "gpio.h"
#include "rpi.h"

// in libpi/include: has useful enums.
#include "rpi-interrupts.h"

const unsigned GPIO_BASE = 0x20200000;
unsigned gpio_ren0 = GPIO_BASE + 0x4C;
unsigned gpio_fen0 = GPIO_BASE + 0x58;
unsigned gpio_peds0 = GPIO_BASE + 0x40;

// returns 1 if there is currently a GPIO_INT0 interrupt,
// 0 otherwise.
//
// note: we can only get interrupts for <GPIO_INT0> since the
// (the other pins are inaccessible for external devices).
int gpio_has_interrupt(void) {
  // p113: IRQ 49 has this
  // p115: IRQ_pending_2 has the value
  dev_barrier();
  unsigned val = GET32(IRQ_pending_2) & (0b1 << (49 - 32));
  dev_barrier();
  if (val != 0)
    return 1;
  return 0;
}

// p97 set to detect rising edge (0->1) on <pin>.
// as the broadcom doc states, it  detects by sampling based on the clock.
// it looks for "011" (low, hi, hi) to suppress noise.  i.e., its triggered only
// *after* a 1 reading has been sampled twice, so there will be delay.
// if you want lower latency, you should us async rising edge (p99)
void gpio_int_rising_edge(unsigned pin) {
  if (pin >= 32)
    return;
  dev_barrier();
  // p97: GPRENn has this
  OR32(gpio_ren0, 0b1 << pin);
  dev_barrier();
  // enable irq_2
  PUT32(IRQ_Enable_2, 0b1 << (49 - 32));
  dev_barrier();
}

// p98: detect falling edge (1->0).  sampled using the system clock.
// similarly to rising edge detection, it suppresses noise by looking for
// "100" --- i.e., is triggered after two readings of "0" and so the
// interrupt is delayed two clock cycles.   if you want  lower latency,
// you should use async falling edge. (p99)
void gpio_int_falling_edge(unsigned pin) {
  if (pin >= 32)
    return;
  dev_barrier();
  // p97: GPRENn has this
  OR32(gpio_fen0, 0b1 << pin);
  dev_barrier();
  // enable irq_2
  PUT32(IRQ_Enable_2, 0b1 << (49 - 32));
  dev_barrier();
}

// p96: a 1<<pin is set in EVENT_DETECT if <pin> triggered an interrupt.
// if you configure multiple events to lead to interrupts, you will have to
// read the pin to determine which caused it.
int gpio_event_detected(unsigned pin) {
  if (pin >= 32)
    return 0;
  dev_barrier();
  int val = GET32(gpio_peds0) & (1 << pin);
  dev_barrier();
  return val;
}

// p96: have to write a 1 to the pin to clear the event.
void gpio_event_clear(unsigned pin) {
  if (pin >= 32)
    return;
  dev_barrier();
  PUT32(gpio_peds0, 1 << pin);
  dev_barrier();
}
