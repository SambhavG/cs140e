// engler, cs140e: starter code for trivial threads package.
#include "rpi.h"
// asdf
#include "rpi-thread.h"

//***********************************************************
// debugging code: tracing and redzone checking.

// if you want to turn off tracing, just
// change to "if 0"
#if 1
#define th_trace(args...) trace(args)
#else
#define th_trace(args...)                                                      \
  do {                                                                         \
  } while (0)
#endif

// if you want to turn off redzone checking,
// change to "if 0"
#if 1
#include "redzone.h"
#define RZ_CHECK() redzone_check(0)
#else
#define RZ_CHECK()                                                             \
  do {                                                                         \
  } while (0)
#endif

/******************************************************************
 * datastructures used by the thread code.
 *
 * you don't have to modify this.
 */

#define E rpi_thread_t
#include "libc/Q.h"
#include "libc/pQ.h"

// currently only have a single run queue and a free queue.
// the run queue is FIFO.
static Q_t freeq;
static pQ_t runq;
static rpi_thread_t *cur_thread; // current running thread.
static int cur_priority;
static rpi_thread_t *scheduler_thread; // first scheduler thread.

// monotonically increasing thread id: won't wrap before reboot :)
static unsigned tid = 1;

/******************************************************************
 * simplistic pool of thread blocks: used to make alloc/free
 * faster (plus, our kmalloc doesn't have free (other than reboot).
 *
 * you don't have to modify this.
 */

// total number of thread blocks we have allocated.
static unsigned nalloced = 0;

// keep a cache of freed thread blocks.  call kmalloc if run out.
static rpi_thread_t *th_alloc(void) {
  RZ_CHECK();
  rpi_thread_t *t = Q_pop(&freeq);

  if (!t) {
    t = kmalloc_aligned(sizeof *t, 8);
    nalloced++;
  }
#define is_aligned(_p, _n) (((unsigned)(_p)) % (_n) == 0)
  demand(is_aligned(&t->stack[0], 8), stack must be 8 - byte aligned !);
  t->tid = tid++;
  return t;
}

static void th_free(rpi_thread_t *th) {
  RZ_CHECK();
  // push on the front in case helps with caching.
  Q_push(&freeq, th);
}

/*****************************************************************
 * implement the code below.
 */

// stack offsets we expect.
//  - see <code-asm-checks/5-write-regs.c>
enum {
  R4_OFFSET = 0,
  R5_OFFSET,
  R6_OFFSET,
  R7_OFFSET,
  R8_OFFSET,
  R9_OFFSET,
  R10_OFFSET,
  R11_OFFSET,
  R14_OFFSET = 8,
  LR_OFFSET = 8
};

// return pointer to the current thread.
rpi_thread_t *rpi_cur_thread(void) {
  assert(cur_thread);
  RZ_CHECK();
  return cur_thread;
}

int rpi_cur_priority(void) {
  assert(cur_thread);
  RZ_CHECK();
  return cur_priority;
}

// create a new thread.
rpi_thread_t *rpi_fork(void (*code)(void *arg), void *arg) {
  RZ_CHECK();
  rpi_thread_t *t = th_alloc();

  // write this so that it calls code,arg.
  void rpi_init_trampoline(void);

  unsigned near_end_of_stack = THREAD_MAXSTACK - LR_OFFSET - 1;

  t->stack[near_end_of_stack + LR_OFFSET] = (uint32_t)&rpi_init_trampoline;
  t->stack[near_end_of_stack + R4_OFFSET] = (uint32_t)code;
  t->stack[near_end_of_stack + R5_OFFSET] = (uint32_t)arg;
  t->saved_sp = (uint32_t *)&(t->stack[near_end_of_stack]);

  // should check that <t->saved_sp> points within the
  // thread stack.
  th_trace("rpi_fork: tid=%d, code=[%p], arg=[%x], saved_sp=[%p]\n", t->tid,
           code, arg, t->saved_sp);
  pQ_insert(&runq, t, 0);

  return t;
}

rpi_thread_t *rpi_fork_with_priority(void (*code)(void *arg), void *arg,
                                     int priority) {
  RZ_CHECK();
  rpi_thread_t *t = th_alloc();

  // write this so that it calls code,arg.
  void rpi_init_trampoline(void);

  unsigned near_end_of_stack = THREAD_MAXSTACK - LR_OFFSET - 1;

  t->stack[near_end_of_stack + LR_OFFSET] = (uint32_t)&rpi_init_trampoline;
  t->stack[near_end_of_stack + R4_OFFSET] = (uint32_t)code;
  t->stack[near_end_of_stack + R5_OFFSET] = (uint32_t)arg;
  t->saved_sp = (uint32_t *)&(t->stack[near_end_of_stack]);

  // should check that <t->saved_sp> points within the
  // thread stack.
  th_trace("rpi_fork: tid=%d, code=[%p], arg=[%x], saved_sp=[%p]\n", t->tid,
           code, arg, t->saved_sp);
  pQ_insert(&runq, t, priority);

  return t;
}

// exit current thread.
//   - if no more threads, switch to the scheduler.
//   - otherwise context switch to the new thread.
//     make sure to set cur_thread correctly!
void rpi_exit(int exitcode) {
  RZ_CHECK();
  rpi_thread_t *old_thread = cur_thread;
  th_free(cur_thread);

  if (!pQ_empty(&runq)) {
    cur_priority = pQ_top(&runq).priority;
    cur_thread = pQ_pop(&runq).elem_ptr;
    rpi_cswitch(&(old_thread->saved_sp), cur_thread->saved_sp);
  }

  // if you switch back to the scheduler thread:
  th_trace("done running threads, back to scheduler\n");
  rpi_cswitch(&(old_thread->saved_sp), scheduler_thread->saved_sp);

  // should never return.
  not_reached();
}

// yield the current thread.
//   - if the runq is empty, return.
//   - otherwise:
//      * add the current thread to the back
//        of the runq (Q_append)
//      * context switch to the new thread.
//        make sure to set cur_thread correctly!
void rpi_yield(void) {
  RZ_CHECK();
  if (pQ_empty(&runq))
    return;

  pQ_insert(&runq, cur_thread, cur_priority - 1);
  rpi_thread_t *old = cur_thread;
  cur_priority = pQ_top(&runq).priority;
  rpi_thread_t *t = pQ_pop(&runq).elem_ptr;
  cur_thread = t;
  th_trace("switching from tid=%d to tid=%d\n", old->tid, t->tid);
  rpi_cswitch(&(old->saved_sp), t->saved_sp);
}

/*
 * starts the thread system.
 * note: our caller is not a thread!  so you have to
 * create a fake thread (assign it to scheduler_thread)
 * so that context switching works correctly.   your code
 * should work even if the runq is empty.
 */
void rpi_thread_start(void) {
  RZ_CHECK();
  th_trace("starting threads!\n");

  // no other runnable thread: return.
  if (pQ_empty(&runq)) {
    goto end;
  }
  // setup scheduler thread block.
  if (!scheduler_thread)
    scheduler_thread = th_alloc();

  cur_priority = pQ_top(&runq).priority;
  cur_thread = pQ_pop(&runq).elem_ptr;
  rpi_cswitch(&(scheduler_thread->saved_sp), cur_thread->saved_sp);

end:
  RZ_CHECK();
  // if not more threads should print:
  th_trace("done with all threads, returning\n");
}

// helper routine: can call from assembly with r0=sp and it
// will print the stack out.  it then exits.
// call this if you can't figure out what is going on in your
// assembly.
void rpi_print_regs(uint32_t *sp) {
  // use this to check that your offsets are correct.
  printk("cur-thread=%d\n", cur_thread->tid);
  printk("sp=%p\n", sp);

  // stack pointer better be between these.
  printk("stackstart=%p\n", &cur_thread->stack[0]);
  printk("stack=%p\n", &cur_thread->stack[THREAD_MAXSTACK]);
  assert(sp <= &cur_thread->stack[THREAD_MAXSTACK]);
  assert(sp >= &cur_thread->stack[0]);
  for (unsigned i = 0; i < 9; i++) {
    unsigned r = i == 8 ? 14 : i + 4;
    printk("sp[%d]=r%d=%x\n", i, r, sp[i]);
  }
  clean_reboot();
}
