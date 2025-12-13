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
#include "full-except.h"
#include "rpi.h"

// we have a single handler: so just use globals.
static watch_handler_t watchpt_handler = 0;
static void *watchpt_data = 0;
static void *watchpoint_addr;

// is it a load fault?
static int mini_watch_load_fault(void) { return datafault_from_ld(); }

// if we have a dataabort fault, call the watchpoint
// handler.
static void watchpt_fault(regs_t *r) {
  // watchpt handler.
  if (was_brkpt_fault())
    panic("should only get debug faults!\n");
  if (!was_watchpt_fault())
    panic("should only get watchpoint faults!\n");
  if (!watchpt_handler)
    panic("watchpoint fault without a fault handler\n");

  watch_fault_t w = {0};

  // todo("setup the <watch_fault_t> structure");
  // todo("call: watchpt_handler(watchpt_data, &w);");
  w.fault_pc = watchpt_fault_pc();
  w.is_load_p = mini_watch_load_fault();
  w.regs = r;

  uint32_t b = cp14_wcr0_get();
  b = bits_get(b, 5, 8);
  unsigned offset = 0;
  unsigned i;
  for (i = 0; i < 4; i++) {
    if (b == 1)
      break;
    b >>= 1;
  }

  w.fault_addr = (void *)(cp14_wvr0_get() + i);

  watchpt_handler(watchpt_data, &w);

  cp14_wcr0_disable();
  assert(!cp14_wcr0_is_enabled());

  // in case they change the regs.
  switchto(w.regs);
}

// setup:
//   - exception handlers,
//   - cp14,
//   - setup the watchpoint handler
// (see: <1-watchpt-test.c>
void mini_watch_init(watch_handler_t h, void *data) {
  // todo("setup cp14 and <watchpt_fault> using full_except");
  full_except_install(0);
  full_except_set_data_abort(watchpt_fault);
  cp14_enable();
  uint32_t b = 0;
  b = bit_clr(b, 20);      // disable linking
  b = bits_clr(b, 14, 15); // watchpoint matches in secure or non-secure world
  b = bits_set(b, 5, 8,
               0b0000); // watchpoint is triggered for any access 0x0,1,2,3
  b = bits_set(b, 3, 4, 0b11); // for both loads and stores
  b = bits_set(b, 1, 2, 0b11); // for both user and privileged
  // b = bit_set(b, 0); //enable watchpoint
  cp14_wcr0_set(b);

  // just started, should not be enabled.
  assert(!cp14_bcr0_is_enabled());
  assert(!cp14_bcr0_is_enabled());

  watchpt_handler = h;
  watchpt_data = data;
}

// set a watch-point on <addr>.
void mini_watch_addr(void *addr) {
  cp14_wvr0_set((uint32_t)addr);
  uint32_t bottom_bits = (unsigned)addr & 0b11;

  uint32_t b = cp14_wcr0_get();
  b = bits_set(b, 5, 8, 0b0001 << bottom_bits);
  b = bit_set(b, 0);
  cp14_wcr0_set(b);

  assert(cp14_wcr0_is_enabled());
  prefetch_flush();
  watchpoint_addr = addr;
}

// disable current watchpoint <addr>
void mini_watch_disable(void *addr) { cp14_wcr0_disable(); }

// return 1 if enabled.
int mini_watch_enabled(void) { return cp14_wcr0_is_enabled(); }

// called from exception handler: if the current
// fault is a watchpoint, return 1
int mini_watch_is_fault(void) { return was_watchpt_fault(); }
