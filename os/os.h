#ifndef __EQX_OS_H__
#define __EQX_OS_H__


//libpi
#include "rpi.h"
#include "full-except.h"
#include "breakpoint.h"
#include "cpsr-util.h"
#include "asm-helpers.h"

//os
#include "eqx-threads.h"
#include "eqx-internal.h"
#include "eqx-syscalls.h"
#include "queue-ext-T.h"
#include "small-prog.h"
#include "timer-int.h"
#include "armv6-coprocessor-asm.h"

//vm
#include "vm/memmap-default.h"


// #include "rpi-interrupts.h"

// bundle all the configuration stuff in one
// structure
typedef struct {
#if 0
    // if != 0: randomly flip a coin and 
    // Pr(1/random_switch) switch to another 
    // process in ss handler.
    uint32_t random_switch;

    // if 1: turn on cache [this will take work]
    unsigned do_caches_p:1;

    // if 1: turn vm off and on in the ss handler.
    unsigned do_vm_off_on:1;

    // if 1, randomly add to head or tail of runq.
    unsigned do_random_enq:1;

    unsigned emit_exit_p:1; // emit stats on exit.

    unsigned no_vm_p:1; // no VM
#endif
#if 0
    unsigned verbose_p:1,
             // if you run w/ icache on: have to flush the data cache and icache
             // when modify code.
             icache_on_p:1,
             compute_hash_p:1,
             vm_off_p:1,
             run_one_p:1,
             hash_must_exist_p:1,
             disable_asid_p:1;

    unsigned debug_level;     // can be set w/ a runtime config.
    unsigned ramMB;           // default is 128MB
#endif
    // verbose debugging info.
    unsigned verbose_p:1,
             no_user_access_p:1,

             // exclusive: use pin, use pt, or use nothing.
             vm_off_p:1,    // implies the others are off.
             vm_use_pin_p:1,
             vm_use_pt_p:1

            ;
    unsigned ramMB;           // default is 128MB

    // unsigned user_idx;
} eqx_config_t;

extern eqx_config_t eqx_config;
void eqx_init_config(eqx_config_t c);

static inline void clean_dcache(void);
static inline void tlb_flush_asid(uint32_t asid);
void interrupt_full_except(regs_t *r);
static int eqx_check_sp(eqx_th_t *th);
static void eqx_regs_init(eqx_th_t *th);
static __attribute__((noreturn)) void eqx_run_current(void);
eqx_th_t *eqx_fork_stack(void (*fn)(void *), void *arg, void *stack, uint32_t nbytes);
eqx_th_t *eqx_fork(void (*fn)(void *), void *arg);
eqx_th_t *eqx_fork_nostack(void (*fn)(void *), void *arg);
static __attribute__((noreturn)) void eqx_pick_next_and_run(void);
static __attribute__((noreturn)) void sys_exit(eqx_th_t *th, int exitcode);
static int equiv_syscall_handler(regs_t *r);
void sec_alloc_init(unsigned n);
static int sec_is_legal(uint32_t s);
static int sec_alloc_exact_1mb(uint32_t s);
static int sec_alloc_exact_16mb(uint32_t s);
static int sec_is_alloced(uint32_t pa);
static long sec_alloc(void);
static long sec_free(uint32_t s);
static void init_asid_map(void);
static uint32_t get_free_asid(void);
static void free_asid(uint32_t asid);
static void pin_map(unsigned idx, uint32_t va, uint32_t pa, pin_t attr);
void pin_ident(unsigned idx, uint32_t addr, pin_t attr);
static void vm_switch(eqx_th_t *th);
static void vm_init(void);
static void vm_on(uint32_t asid);
static void vm_off(void);
uint32_t eqx_run_threads(void);
void eqx_init_config(eqx_config_t c);
void eqx_init(void);
static inline map_t map_mk(uint32_t va, uint32_t pa, pin_t attr);
static inline uint32_t sec_to_addr(uint32_t sec);
eqx_th_t* eqx_exec_internal(struct prog *prog);
#endif
