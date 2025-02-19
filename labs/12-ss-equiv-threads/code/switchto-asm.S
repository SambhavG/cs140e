#include "rpi-asm.h"

@ all of these are called from <switchto.h>: look there for more
@ help in understanding if need to.

@ drop in your code from last lab to get sp and lr.
@
@ void priv_get_sp_lr_asm(uint32_t mode, uint32_t *sp, uint32_t *lr);
@   - assumes caller is at privileged mode [we could check this]
@   - mode in r0
@   - sp pointer in r1
@   - lr pointer in r2
@
MK_FN(priv_get_sp_lr_asm)
    @ b staff_priv_get_sp_lr_asm
    msr cpsr_c, r0
    prefetch_flush(r3);

    stm r1, {sp}
    stm r2, {lr}

    cps #SUPER_MODE
    prefetch_flush(r3);
    bx lr
    asm_not_reached();


@ drop in your code from lab 13 [in 1-rfe-asm.S]
@
@ void switchto_user_asm(regs_t *r)  __attribute__((noreturn));
@  - <r> is a 17-entry register entry in r0.
@
@ switch to user mode loading r0-r15, and cpsr from <r>
MK_FN(switchto_user_asm)
    @ b staff_switchto_user_asm
    mov sp, r0
    ldm r0, {r0-r14}^
    add sp, #60
    rfe sp
    asm_not_reached();

@ drop in your code from lab 13
@ 
@ void switchto_priv_asm(regs_t *r)  __attribute__((noreturn));
MK_FN(switchto_priv_asm)
    ldr r1, [r0, #64]   @ 0x40
    msr cpsr, r1
    prefetch_flush(r2)
    ldm r0, {r0-r15}
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_priv_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_priv_asm)
    @ initially just forward to ours.
    @ b staff_cswitchto_priv_asm

    @ to implement:
    @ 1. save all the callee regs (just like in your threads library)
    @    into the 17-entry register array we have been using.  note
    @    these have to be at the right offsets.
    @    e.g.:
    @       str r4,  [r0, #(4*4)]  @ r4
    @       str r5,  [r0, #(5*4)]  @ r5
    @       ... etc ...
    @   don't forget the cpsr!
    @ mov     r3, #0
    @ str     r3, [r0]
    @ str     r3, [r0, #4]
    @ str     r3, [r0, #8]
    @ str     r3, [r0, #12]
    @ str     r3, [r0, #48]   @ 0x30
    str     r4, [r0, #16]
    str     r5, [r0, #20]
    str     r6, [r0, #24]
    str     r7, [r0, #28]
    str     r8, [r0, #32]
    str     r9, [r0, #36]   @ 0x24
    str     sl, [r0, #40]   @ 0x28
    str     fp, [r0, #44]   @ 0x2c
    str     sp, [r0, #52]   @ 0x34
    str     lr, [r0, #56]   @ 0x38
    str     lr, [r0, #60]   @ 0x3c
    mrs     r3, CPSR
    str     r3, [r0, #64]   @ 0x40
    mov     r0, r1


    @ 2. just call the right switch routine: note, we should
    @    *not* reach this location when switching back!
    bl switchto_priv_asm
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_user_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_user_asm)
    @ initially just forward to ours.
    @ b staff_cswitchto_user_asm

    @ mov     r3, #0
    @ str     r3, [r0]
    @ str     r3, [r0, #4]
    @ str     r3, [r0, #8]
    @ str     r3, [r0, #12]
    @ str     r3, [r0, #48]   @ 0x30
    str     r4, [r0, #16]
    str     r5, [r0, #20]
    str     r6, [r0, #24]
    str     r7, [r0, #28]
    str     r8, [r0, #32]
    str     r9, [r0, #36]   @ 0x24
    str     sl, [r0, #40]   @ 0x28
    str     fp, [r0, #44]   @ 0x2c
    str     sp, [r0, #52]   @ 0x34
    str     lr, [r0, #56]   @ 0x38
    str     lr, [r0, #60]   @ 0x3c
    mrs     r3, CPSR
    str     r3, [r0, #64]   @ 0x40
    mov     r0, r1

    bl switchto_user_asm
    asm_not_reached();
