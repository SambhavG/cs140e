#include "eqx-os.h"

// Cache and TLB maintenance functions for fork()
// Clean and invalidate data cache - ensures all dirty data is written to RAM
static inline void clean_dcache(void) {
  uint32_t r = 0;
  // Clean and invalidate entire data cache (mcr p15, 0, r, c7, c14, 0)
  asm volatile("mcr p15, 0, %0, c7, c14, 0" ::"r"(r));
  // Data synchronization barrier - ensures cache operation completes
  asm volatile("mcr p15, 0, %0, c7, c10, 4" ::"r"(r));
}

// Flush entire TLB - invalidate all TLB entries
static inline void tlb_flush_all(void) {
  uint32_t r = 0;
  // Invalidate entire unified TLB (mcr p15, 0, r, c8, c7, 0)
  asm volatile("mcr p15, 0, %0, c8, c7, 0" ::"r"(r));
  // Data synchronization barrier - ensures TLB operation completes
  asm volatile("mcr p15, 0, %0, c7, c10, 4" ::"r"(r));
  // Prefetch flush - ensures TLB changes are visible to instruction fetch
  prefetch_flush();
}

// check for initialization bugs.
static int eqx_init_p = 0;
static unsigned ntids = 1;
int eqx_verbose_p = 1;

// simple thread queue.
//  - should make so you can delete from the middle.
typedef struct rq {
  eqx_th_t *head, *tail;
} rq_t;

// will define eqx_pop, eqx_push, eqx_append, etc
gen_queue_T(eqx_th, rq_t, head, tail, eqx_th_t, next) static rq_t eqx_runq;

// pointer to current thread.  not null when running
// threads, null when not.
static eqx_th_t *volatile cur_thread;

// just like in the interleave lab (10): where we switch back to.
static regs_t start_regs;

// check that the <sp> stack pointer reg is within
// the thread stack.  just die if not.
static int eqx_check_sp(eqx_th_t *th) {
  let sp = th->regs.regs[REGS_SP];
  if (sp < th->stack_start)
    panic("stack underflow: sp=%x, lowest legal=%x\n", sp, th->stack_start);
  if (sp > th->stack_end)
    panic("stack overflow: sp=%x, highest legal=%x\n", sp, th->stack_end);
  return 1;
}

// initialize <th>'s registers so that:
//   - it runs <th->fn> with argument <th->arg> at USER
//     level.
//   - if <th->fn> returns, it will transparently call
//     <sys_equiv_exit>.
//
// similar to the thread initialization in our
// check-interleave code.
static void eqx_regs_init(eqx_th_t *th) {
  // calculate thread <cpsr>
  //  1. default: get the current <cpsr>, so that we keep
  //    the interrupt status etc) and:
  //  2. clear the carry flags since they have nothing
  //     to do with the thread.
  //  3. set the mode to USER so can single step.
  //     caller can change this when we return if desired.
  uint32_t cpsr = cpsr_inherit(USER_MODE, cpsr_get());

  // initialize the thread block
  //  see <switchto.h>
  //
  // Note, all registers not set will = 0.
  regs_t r = {
      .regs[REGS_PC] = (uint32_t)th->fn,
      // the first argument to fn
      .regs[REGS_R0] = (uint32_t)th->arg,

      // stack pointer register
      .regs[REGS_SP] = (uint32_t)th->stack_end,
      // the cpsr to use
      .regs[REGS_CPSR] = cpsr,

      // where to jump to if the code returns.
      .regs[REGS_LR] = (uint32_t)sys_equiv_exit,
  };

  th->regs = r;
  if (!eqx_check_sp(th))
    panic("stack is out of bounds!\n");
}

// Run the given thread until it traps back (syscall/exception) OR exits.
// In "run-to-completion" mode, the only scheduling decision is in sys_exit().
static __attribute__((noreturn)) void eqx_run_current(void) {
  assert(cur_thread);
  // Ensure the pinned mappings correspond to cur_thread before executing it.
  vm_switch(cur_thread);
  prefetch_flush();
  switchto(&cur_thread->regs);
  not_reached();
}

// fork <fn(arg)> as a pre-emptive thread and allow the caller
// to allocate the stack.
//   - <stack>: the base of the stack (8 byte aligned).
//   - <nbytes>: the size of the stack.
eqx_th_t *eqx_fork_stack(void (*fn)(void *), void *arg, void *stack,
                         uint32_t nbytes) {

  eqx_th_t *th = kmalloc(sizeof *th);
  // printk("kmalloc'd th=%p\n", th);
  th->fn = (uint32_t)fn;
  th->arg = (uint32_t)arg;

  // we do a dumb monotonic thread id [1,2,...]
  th->tid = ntids++;

  // stack grows down: must be 8-byte aligned.
  th->stack_start = (uint32_t)stack;
  th->stack_end = th->stack_start + nbytes;
  // check stack alignment.
  unsigned rem = (uint32_t)th->stack_end % 8;
  if (rem)
    panic("stack is not 8 byte aligned: mod 8 = %d\n", rem);

  eqx_regs_init(th);
  eqx_th_push(&eqx_runq, th);
  return th;
}

// fork + allocate a 8-byte aligned stack.
eqx_th_t *eqx_fork(void (*fn)(void *), void *arg) {

  void *stack = kmalloc_aligned(eqx_stack_size, 8);
  assert((uint32_t)stack % 8 == 0);

  return eqx_fork_stack(fn, arg, stack, eqx_stack_size);
}

// fork with no stack: this is used as a debugging
// aid to support the easiest case of routines
// that just do ALU operations and do not use a stack.
eqx_th_t *eqx_fork_nostack(void (*fn)(void *), void *arg) {
  return eqx_fork_stack(fn, arg, 0, 0);
}

// In run-to-completion mode, we do NOT time-slice.
// The only "scheduler" action is to pick the next thread after exit.
static __attribute__((noreturn)) void eqx_pick_next_and_run(void) {
  // Choose the next runnable.
  cur_thread = eqx_th_pop(&eqx_runq);
  if (!cur_thread) {
    // No runnable threads: return to kernel/start_regs.
    switchto(&start_regs);
  }
  eqx_run_current();
  not_reached();
}

/****************************************************************
 * system calls.
 */

static __attribute__((noreturn)) void sys_exit(eqx_th_t *th, int exitcode) {
  // eqx_trace("thread=%d exited with code=%d\n", th->tid, exitcode);
  eqx_pick_next_and_run();
}

// our two system calls:
//   - exit: get the next thread if there is one.
//   - putc: so we can handle race conditions with prints
static int equiv_syscall_handler(regs_t *r) {
  if (interrupts_on_p())
    panic("interrupts are enabled!\n");

  // sanity checking.
  //  - mode routines in <libpi/include/cpsr-util.h>
  //
  // 1. the SPSR better = what we saved!
  uint32_t spsr = spsr_get();
  assert(r->regs[REGS_CPSR] == spsr);
  // 2. it better be either USER or (later) SYSTEM
  assert(mode_get(spsr) == USER_MODE);
  // 3. since running, should have non-null <cur_thread>
  let th = cur_thread;
  assert(th);
  // 4. check the stack to see if sp is in-bounds.
  eqx_check_sp(th);

  // save the current registers in case we switch.
  th->regs = *r;

  unsigned sysno = r->regs[0];
  switch (sysno) {

  case EQX_SYS_EXIT: {
    sys_exit(th, r->regs[1]);
    not_reached();
  }
  case EQX_SYS_PUTC: {
    uart_put8(r->regs[1]);
    break;
  }
  case EQX_SYS_FORK: {
    clean_dcache();

    vm_off();
    eqx_th_t *child = kmalloc(sizeof(eqx_th_t));
    memcpy(child, th, sizeof(eqx_th_t));
    child->tid = ntids++;

    uint32_t new_code_pa = 0, new_data_pa = 0;
    if (th->code_pin.pa) {
      new_code_pa = sec_to_addr(sec_alloc());
    }
    if (th->data_pin.pa) {
      new_data_pa = sec_to_addr(sec_alloc());
    }

    if (th->code_pin.pa) {
      memcpy((void *)new_code_pa, (void *)th->code_pin.pa, MB(1));
      child->code_pin.pa = new_code_pa;
    }
    if (th->data_pin.pa) {
      memcpy((void *)new_data_pa, (void *)th->data_pin.pa, MB(1));
      child->data_pin.pa = new_data_pa;
    }

    clean_dcache();
    prefetch_flush();

    vm_on();

    vm_switch(th);

    // tlb_flush_all();

    child->regs.regs[REGS_R0] = 0;
    th->regs.regs[REGS_R0] = child->tid;

    eqx_th_push(&eqx_runq, child);
    return child->tid;
    break;
  }
  case EQX_SYS_EXEC: {
    vm_off();
    struct prog *p = (void *)r->regs[0];
    eqx_th_t *new_th = eqx_exec_internal(p);
    vm_on();
    new_th->tid = th->tid;
    cur_thread = eqx_th_pop(&eqx_runq);
    if (!cur_thread)
      panic("Exec error: run queue empty after loading new program?\n");
    // IMPORTANT: ensure address space matches the thread we're about to run.
    eqx_run_current();
    not_reached();
  }
  case EQX_SYS_WAITPID: {
    panic("waitpid not implemented\n");
    break;
  }
  case EQX_SYS_SBRK: {
    panic("sbrk not implemented\n");
    break;
  }
  case EQX_SYS_ABORT: {
    panic("abort not implemented\n");
    break;
  }
  case EQX_SYS_GET_CPSR: {
    r->regs[0] = cpsr_get();
    break;
  }
  case EQX_SYS_PUT_HEX: {
    printk("%x", r->regs[1]);
    break;
  }
  case EQX_SYS_PUT_INT: {
    printk("%d", r->regs[1]);
    break;
  }
  case EQX_SYS_PUT_PID: {
    printk("%d", th->tid);
    break;
  }
  case EQX_SYS_GET_PID: {
    return th->tid;
    break;
  }
  default: {
    panic("illegal system call: %d\n", sysno);
    break;
  }
  }

  // Run-to-completion behavior:
  // - For non-exit syscalls, resume the SAME thread immediately.
  // - Make sure any register writes we made to th->regs are reflected back to
  // *r.
  *r = th->regs;
  return 0;
}

//******************************************************
// simple 1MB allocation/deallocation.

enum { MAX_SECS = 512 }; // can't ever be bigger than this.
static uint32_t sections[MAX_SECS];
// actual number of 1mb sections [will be smaller than 512mb]
static uint32_t nsec;

// need to use the mbox to read the
// the amount of actual memory avail.
void sec_alloc_init(unsigned n) {
  assert(n > 0 && n < MAX_SECS);
  nsec = n;
  memset(sections, 0, sizeof sections);
}

// within [0..nsec)
static int sec_is_legal(uint32_t s) { return s < nsec; }

// allocate 1mb section <s> --- currently
// panic if not free.
static int sec_alloc_exact(uint32_t s) {
  assert(sec_is_legal(s));
  if (sections[s])
    return 0;
  // debug("allocating sec=%d\n", s);
  sections[s] = 1;
  return 1;
}

// is physical address <pa> allocated?
static int sec_is_alloced(uint32_t pa) {
  // allocated by the machine.
  // unclear we should do this.
  if (pa >= 0x20000000)
    return 1;

  uint32_t s = pa >> 20;
  if (!s)
    assert(!pa);
  assert(sec_is_legal(s));
  // refcnt not 0 = allocated.
  return sections[s];
}

// allocate a free 1mb section.
static long sec_alloc(void) {
  for (uint32_t i = 0; i < nsec; i++) {
    if (!sections[i]) {
      // debug("allocating sec=%d\n", i);
      sections[i] = 1;
      return i;
    }
  }
  // change this to an error.
  panic("can't allocate any section?\n");
  return -1;
}

// returns refcnt
static long sec_free(uint32_t s) {
  assert(sec_is_legal(s));
  if (!sections[s])
    panic("section %d is not allocated!\n");
  return --sections[s];
}

/**********************************************************************
 * vm code.
 */

// hack to handle linking error.
// __attribute__((weak)) cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void) {
//   return cp15_ctrl_reg1_rd();
// }

// default asid.
enum { ASID = 1 };
// free index
static int pin_idx;

static eqx_config_t config = {.ramMB = 256};

// what is asid?
static void pin_map(unsigned idx, uint32_t va, uint32_t pa, pin_t attr) {
  assert(sec_is_alloced(pa));
  assert(idx < 8);
  pin_mmu_sec(idx, va, pa, attr);
}

// we pin right away.
void pin_ident(unsigned idx, uint32_t addr, pin_t attr) {
  uint32_t secn = addr >> 20;
  assert(sec_is_legal(secn));
  if (!sec_alloc_exact(secn))
    panic("can't allocate sec=%d\n", secn);
  pin_map(idx, addr, addr, attr);
}

// switch address spaces to <th>
static void vm_switch(eqx_th_t *th) {
  // extend as needed.
  if (!config.vm_use_pin_p)
    return;

  tlb_flush_all();

  // right now don't have any pinning: just running an
  // idenity map.
  if (!th->code_pin.va)
    return;
  assert(th->data_pin.va);

  let p = &th->code_pin;
  pin_map(pin_idx, p->va, p->pa, p->attr);
  p = &th->data_pin;
  pin_map(pin_idx + 1, p->va, p->pa, p->attr);
  pin_set_context(ASID);
}

// one time initialization to <th>
static void vm_init(void) {
  // extend as needed.
  if (!config.vm_use_pin_p)
    return;

  sec_alloc_init(config.ramMB);

  // default domain bits.
  // NOTE: if you do anything more fancy, will probably
  // need to move this.
  pin_mmu_init(dom_bits);

  // if we aren't forking user processes, have to mark the code as
  // user accessible.

  uint32_t perm;
  if (config.no_user_access_p) {
    perm = no_user;
  } else {
    perm = user_access;
  }

  unsigned idx = 0;
  pin_t kern = pin_mk_global(dom_kern, perm, MEM_uncached);
  pin_ident(idx++, SEG_CODE, kern);
  pin_ident(idx++, SEG_HEAP, kern);
  pin_ident(idx++, SEG_STACK, kern);
  pin_ident(idx++, SEG_INT_STACK, kern);

  // use 16mb section for device.  we never give users access
  pin_t dev = pin_16mb(pin_mk_global(dom_kern, no_user, MEM_device));
  pin_mmu_sec(idx++, SEG_BCM_0, SEG_BCM_0, dev);

  // next index avail
  pin_idx = idx;

#if 0
    enum { ASID1 = 1, ASID2 = 2 };
    // do a non-ident map
    enum {
        user_addr = MB(16),
        phys_addr1 = user_addr+MB(1),
        phys_addr2 = user_addr+MB(2)
    };
    pin_t user1 = pin_mk_user(dom_kern, ASID1, no_user, MEM_uncached);
    pin_t user2 = pin_mk_user(dom_kern, ASID2, no_user, MEM_uncached);
    pin_mmu_sec(idx++, user_addr, phys_addr1, user1);
    pin_mmu_sec(idx++, user_addr, phys_addr2, user2);
#endif
}

static void vm_on(void) {
  if (!config.vm_use_pin_p)
    return;
  pin_set_context(ASID);
  assert(!mmu_is_enabled());
  pin_mmu_enable();
  assert(mmu_is_enabled());
  // output("mmu on\n");
}

static void vm_off(void) {
  if (!config.vm_use_pin_p) {
    printk("vm_off: vm_use_pin_p is off\n");
    return;
  }
  assert(mmu_is_enabled());
  pin_mmu_disable();
  // output("mmu off\n");
}

/*****************************************************************
 * setup and running the entire code.
 */

// run all threads in the run-queue using
// single step mode.
//   - should make a non-ss version.
//   - make sure you can add timer interrupts.
//   - make sure you can do match faults.
uint32_t eqx_run_threads(void) {
  if (!eqx_init_p)
    panic("did not initialize eqx!\n");

  // for today we don't expect an empty runqueue,
  // but you can certainly get rid of this if prefer.
  cur_thread = eqx_th_pop(&eqx_runq);
  if (!cur_thread)
    panic("empty run queue?\n");

  // start mismatching (we are at privileged mode
  // so won't start til we switch to the first
  // thread).
  // brkpt_mismatch_start();

  // note: we're running the first instruction and
  // *then* getting a mismatch.  if you wanted
  // to mismatch right away use an addresss that
  // should never executed (e.g. 0).
  // brkpt_mismatch_set(cur_thread->regs.regs[15]);

  // setup vm.
  // NOTE: we will potentially do multiple times
  // so need to make sure works in that case.
  vm_on();
  vm_switch(cur_thread);

  // we use <cswitch> so can come back here.
  // NOTE: nothing else better use our current stack!
  switchto_cswitch(&start_regs, &cur_thread->regs);

  // ran all threads: turn off mismatching.
  // brkpt_mismatch_stop();

  // turn off vm
  vm_off();

  // check that runqueue empty.
  assert(!cur_thread);
  assert(!eqx_runq.head);
  assert(!eqx_runq.tail);

  eqx_trace("done running threads\n");
  return 0;
}

// one time initialization.
//  - setup heap if haven't.
//  - install exception handlers [this is overly
//    self-important: in a real system there could
//    be other subsystems that want to do so]
//
//    for other labs (e.g., vm) will need a way to
//    compose these handlers with other ones.
//
// could just fold it into <eqx_run_threads>
void eqx_init_config(eqx_config_t c) {
  if (eqx_init_p)
    panic("called init twice!\n");
  config = c;
  eqx_init_p = 1;

  // initialize the kernel heap if it hasn't been.
  if (!kmalloc_heap_start())
    kmalloc_init(1);

  // install is idempotent if already there.
  full_except_install(0);

  // for breakpoint handling (like lab 10)
  // full_except_set_prefetch(equiv_single_step_handler);
  // for system calls (like many labs)
  full_except_set_syscall(equiv_syscall_handler);

  vm_init();
}

void eqx_init(void) {
  eqx_config_t c = {.ramMB = 256, .vm_use_pin_p = 1};
  return eqx_init_config(c);
}

//******************************************************
// fork exec system calls.

static inline map_t map_mk(uint32_t va, uint32_t pa, pin_t attr) {
  // not aligned.
  assert(!bits_get(va, 0, 19));
  assert(!bits_get(pa, 0, 19));
  return (map_t){.va = va, .pa = pa, .attr = attr};
}

static inline uint32_t sec_to_addr(uint32_t sec) { return (sec << 20); }

eqx_th_t *eqx_exec_internal(struct prog *prog) {
  assert(prog);
  output("EXEC: progname=<%s>, nbytes=%d\n", prog->name, prog->nbytes);
  small_prog_hdr_t s = small_prog_hdr_mk((void *)prog->code);
  // ensure that code and data are aligned to 1MB.
  assert(s.data_addr % MB(1) == 0);
  assert(s.code_addr % MB(1) == 0);

  // small_prog_hdr_print(s);

  void *code_src = (void *)prog->code + s.code_offset;
  void *data_src = (void *)prog->code + s.data_offset;

  let p = eqx_fork_stack((void *)s.code_addr, 0, (void *)s.data_addr,
                         eqx_stack_size);

  // currently: vm not turned on, so we can copy whatever.

  uint32_t data = 0, code = 0;

  // note: if we aren't going to turn on at all,
  // need to map the exact sections.
  if (config.vm_off_p) {
    data = s.data_addr;
    code = s.code_addr;
    panic("what\n");
  } else {
    // we don't care which sectors we use.
    data = sec_to_addr(sec_alloc());
    code = sec_to_addr(sec_alloc());

    pin_t user_attr = pin_mk_user(dom_user, ASID, perm_rw_user, MEM_uncached);
    p->data_pin = map_mk(s.data_addr, data, user_attr);
    p->code_pin = map_mk(s.code_addr, code, user_attr);
  }

  unsigned offset = s.bss_addr - s.data_addr;
  assert(offset < MB(1));

  // this either needs mmu off, or needs to turn it off.
  assert(!mmu_is_enabled());
  gcc_mb();
  memset((void *)data + offset, 0, s.bss_nbytes);
  memcpy((void *)data, data_src, s.data_nbytes);
  memcpy((void *)code, code_src, s.code_nbytes);
  gcc_mb();

  return p;
}
