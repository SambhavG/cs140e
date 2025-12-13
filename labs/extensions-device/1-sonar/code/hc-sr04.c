#include "hc-sr04.h"

static uint32_t get_time_passed(uint32_t start, uint32_t end) {
  uint32_t time_elapsed = end - start;
  if (end < start) {
    time_elapsed = (UINT32_MAX - start) + end;
  }
  return time_elapsed;
}

// gpio_read(pin) until either:
//  1. gpio_read(pin) != v ==> return 1.
//  2. <timeout> microseconds have passed ==> return 0
static int read_while_eq(int pin, int v, unsigned timeout) {
  uint32_t time_start = timer_get_usec();
  int num_not_equal = 0;
  while (1) {
    if (timeout < get_time_passed(time_start, timer_get_usec()))
      return 0;
    if (gpio_read(pin) != v) {
      num_not_equal++;
    }
    if (num_not_equal == 10) {
      return 1;
    }
  }
  return 1;
}

// initialize:
//  1. setup the <trigger> and <echo> GPIO pins.
// 	2. init the HC-SR04 (pay attention to time delays here)
//
// Pay attention to the voltages on:
//    - Vcc
//    - Vout.
//
// Troubleshooting:
// 	1. there are conflicting accounts of what value voltage you
//	need for Vcc.
//
// 	2. the initial 3 page data sheet you'll find sucks; look for
// 	a longer one.
//
// The comments on the sparkfun product page might be helpful.
hc_sr04_t hc_sr04_init(unsigned trigger, unsigned echo) {
  // return staff_hc_sr04_init(trigger, echo);
  hc_sr04_t h = {.trigger = trigger, .echo = echo};

  gpio_set_output(trigger);
  gpio_set_input(echo);

  // give trigger a high 5V pulse for 10 us
  gpio_write(trigger, 1);
  delay_us(12);
  gpio_write(trigger, 0);

  // return staff_hc_sr04_init(trigger, echo);

  return h;
}

// get distance.
//	1. do a send (again, pay attention to any needed time
// 	delays)
//
//	2. measure how long it takes and compute round trip
//	by converting that time to distance using the datasheet
// 	formula
//
// troubleshooting:
//  0. We don't have floating point or integer division.
//
//  1. The pulse can get lost!  Make sure you use the timeout read
//  routine you write.
//
//	2. readings can be noisy --- you may need to require multiple
//	high (or low) readings before you decide to trust the
// 	signal.
//
int hc_sr04_get_distance(hc_sr04_t h, unsigned timeout_usec) {
  // return staff_hc_sr04_get_distance(h, timeout_usec);
  uint32_t trigger = h.trigger;
  uint32_t echo = h.echo;

  gpio_write(trigger, 1);
  delay_us(12);
  gpio_write(trigger, 0);

  // Wait for echo to go from 0 to 1
  int result = read_while_eq(echo, 0, timeout_usec);
  if (!result)
    return -1; // timed out
  // start time
  uint32_t start_time = timer_get_usec();

  // Wait for echo to go from 1 to 0
  result = read_while_eq(echo, 1, 40 * 1000 * 1000);
  if (!result)
    return -1; // timed out
  uint32_t end_time = timer_get_usec();

  uint32_t time_elapsed = get_time_passed(start_time, end_time);
  if (time_elapsed > 25000) {
    return -1;
  }

  uint32_t num_inches = time_elapsed / 148;
  return num_inches;
  // return staff_hc_sr04_get_distance(h, timeout_usec);
}
