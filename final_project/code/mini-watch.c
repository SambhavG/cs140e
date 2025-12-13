// very dumb, simple interface to wrap up watchpoints better.
// only handles a single watchpoint.
//
// You should be able to take most of the code from the
// <1-watchpt-test.c> test case where you already did
// all the needed operations.  This interface just packages
// them up a bit.
//
// possible extensions:
//   - do as many as hardware supports, perhaps a handler for
//     each one.
//   - make fast.
//   - support multiple independent watchpoints so you'd return
//     some kind of structure and pass it in as a parameter to
//     the routines.
#include "mini-watch.h"

// we have a single handler: so just use globals.
// static watch_handler_t watchpt_handler = 0;
// static void *watchpt_data = 0;
void *wp_addr[2] = {0};
int which_wp_on[2] = {0};

static watch_handler_t watchpt_handler[2];
static void *watchpt_data[2];
static int num_watchpoints = 0;

// Tells you which watchpoint is the one looking at this address
static int which_wp(void *addr) {
  for (int i = 0; i < 2; i++) {
    if (which_wp_on[i] && wp_addr[i] == addr) {
      return i;
    }
  }
  return -1;
}

static int get_next_available_wp() {
  for (int i = 0; i < 2; i++) {
    if (!which_wp_on[i])
      return i;
  }
  return -1;
}

static int wp_ctrl_is_enabled(unsigned index) {
  if (index == 0 && cp14_wcr0_is_enabled())
    return 1;
  if (index == 1 && cp14_wcr1_is_enabled())
    return 1;
  return 0;
}

static void wp_ctrl_enable(unsigned index) {
  if (index == 0)
    cp14_wcr0_enable();
  if (index == 1)
    cp14_wcr1_enable();
}

static void wp_ctrl_disable(unsigned index) {
  if (index == 0)
    cp14_wcr0_disable();
  if (index == 1)
    cp14_wcr1_disable();
}

static unsigned wp_ctrl_get(unsigned index) {
  if (index == 0)
    return cp14_wcr0_get();
  if (index == 1)
    return cp14_wcr1_get();
  assert(1 == 0);
}

static void wp_ctrl_set(unsigned index, unsigned val) {
  if (index == 0)
    cp14_wcr0_set(val);
  if (index == 1)
    cp14_wcr1_set(val);
}

static void wp_val_set(unsigned index, void *addr) {
  assert(index < 2);
  if (index == 0)
    cp14_wvr0_set((unsigned)addr);
  if (index == 1)
    cp14_wvr1_set((unsigned)addr);
}

// is it a load fault?
static int mini_watch_load_fault(void) { return datafault_from_ld(); }

// if we have a dataabort fault, call the watchpoint
// handler.
void watchpt_fault(regs_t *r) {
  void *fault_addr = (void *)cp15_far_get();
  printk("[watchpt_fault] Got watchpoint fault on address %x\n", fault_addr);
  int triggered_watchpoint = which_wp(fault_addr);

  // watchpt handler.
  if (was_brkpt_fault())
    panic("should only get debug faults!\n");
  if (!was_watchpt_fault())
    panic("should only get watchpoint faults!\n");
  if (!watchpt_handler[triggered_watchpoint])
    panic("watchpoint fault without a fault handler\n");

  watch_fault_t w = {0};

  w.fault_pc = watchpt_fault_pc();
  w.is_load_p = mini_watch_load_fault();
  w.regs = r;
  w.fault_addr = fault_addr;

  watchpt_handler[triggered_watchpoint](watchpt_data[triggered_watchpoint], &w);

  // Disable whichever watchpoint was triggered

  wp_ctrl_disable(triggered_watchpoint);
  which_wp_on[triggered_watchpoint] = 0;
  num_watchpoints--;

  assert(!wp_ctrl_is_enabled(triggered_watchpoint));

  // in case they change the regs.
  switchto(w.regs);
}

// setup:
//   - exception handlers,
//   - cp14,
//   - setup the watchpoint handler
// (see: <1-watchpt-test.c>
void mini_watch_init() {
  full_except_install(0);
  full_except_set_data_abort(watchpt_fault);
  cp14_enable();
}

// set a watch-point on <addr>.
void mini_watch_addr(void *addr, watch_handler_t h, void *data) {
  assert(num_watchpoints < 2);
  addr = (void *)((unsigned)addr & ~0b11);

  int this_wp_index = get_next_available_wp();
  which_wp_on[this_wp_index] = 1;

  // Set up watchpoint (originally in mini_watch_input)
  uint32_t b = 0;
  b = bit_clr(b, 20);      // disable linking
  b = bits_clr(b, 14, 15); // watchpoint matches in secure or non-secure world
  b = bits_set(b, 5, 8,
               0b0000); // watchpoint is triggered for any access 0x0,1,2,3
  b = bits_set(b, 3, 4, 0b11); // for both loads and stores
  b = bits_set(b, 1, 2, 0b11); // for both user and privileged
  b = bits_set(b, 5, 8, 0b1111);
  b = bit_set(b, 0);
  wp_val_set(num_watchpoints, addr);
  wp_ctrl_set(num_watchpoints, b);

  watchpt_handler[this_wp_index] = h;
  watchpt_data[this_wp_index] = data;
  wp_addr[this_wp_index] = addr;

  assert(wp_ctrl_is_enabled(this_wp_index));
  num_watchpoints++;
  prefetch_flush();
}

// disable current watchpoint <addr>
void mini_watch_disable(void *addr) {
  addr = (void *)((unsigned)addr & ~0b11);
  int index = which_wp(addr);
  assert(index != -1);
  which_wp_on[index] = 0;
  num_watchpoints--;
  wp_ctrl_disable(index);
}

int mini_watch_enabled() { return num_watchpoints; }

// called from exception handler: if the current
// fault is a watchpoint, return 1
int mini_watch_is_fault(void) { return was_watchpt_fault(); }
