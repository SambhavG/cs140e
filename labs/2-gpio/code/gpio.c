/*
 * Implement the following routines to set GPIO pins to input or
 * output, and to read (input) and write (output) them.
 *  - DO NOT USE loads and stores directly: only use GET32 and
 *    PUT32 to read and write memory.
 *  - DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See rpi.h in this directory for the definitions.
 */
#include "rpi.h"

// see broadcomm documents for magic addresses.
//
// if you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
  GPIO_BASE = 0x20200000,
  GPIO_fsel0 = GPIO_BASE,
  GPIO_fsel_offset = 4,
  gpio_set0 = (GPIO_BASE + 0x1C),
  gpio_clr0 = (GPIO_BASE + 0x28),
  gpio_lev0 = (GPIO_BASE + 0x34)

  // <you may need other values.>
};

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// note: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use array calculations!
void gpio_set_output(unsigned pin) {
  if (pin >= 32 && pin != 47)
    return;

  // implement this
  // use <gpio_fsel0>

  uint32_t bit = 1;
  uint32_t addr = GPIO_BASE;
  uint32_t mask = 0b111;

  // number of offsets is pin / 10
  addr += GPIO_fsel_offset * (pin / 10);

  bit <<= 3 * (pin % 10);
  mask <<= 3 * (pin % 10);

  // Get addr, mask it, | with bit, put
  uint32_t cur = GET32(addr);
  cur &= ~mask;
  cur |= bit;

  PUT32(addr, cur);
}

// set GPIO <pin> on.
void gpio_set_on(unsigned pin) {
  if (pin >= 32 && pin != 47)
    return;
  // implement this
  // use <gpio_set0>

  // Write 1 to gpioset0 left shifted by pin
  if (pin <= 31) {
    PUT32(gpio_set0, ((unsigned)1) << pin);
  } else {
    PUT32(gpio_set0 + 4, ((unsigned)1) << (pin - 32));
  }
}

// set GPIO <pin> off
void gpio_set_off(unsigned pin) {
  if (pin >= 32 && pin != 47)
    return;
  // implement this
  // use <gpio_clr0>
  if (pin <= 31) {
    PUT32(gpio_clr0, ((unsigned)1) << pin);
  } else {
    PUT32(gpio_clr0 + 4, ((unsigned)1) << (pin - 32));
  }
}

// set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
  if (v)
    gpio_set_on(pin);
  else
    gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> to input.
void gpio_set_input(unsigned pin) {
  uint32_t bit = 1;
  uint32_t addr = GPIO_BASE;
  uint32_t mask = 0b111;

  // number of offsets is pin / 10
  addr += GPIO_fsel_offset * (pin / 10);

  mask <<= 3 * (pin % 10);

  // Get addr, mask it, | with bit, put
  uint32_t cur = GET32(addr);
  cur &= ~mask;

  PUT32(addr, cur);
}

// return the value of <pin>
int gpio_read(unsigned pin) {
  unsigned v = 0;

  if (pin <= 31) {
    v = GET32(gpio_lev0);
  } else {
    pin -= 32;
    v = GET32(gpio_lev0 + 4);
  }
  v &= ((unsigned)1) << pin;
  v >>= pin;

  return v;
}
