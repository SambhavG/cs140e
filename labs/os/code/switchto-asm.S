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
    mrs r3, CPSR              @ save current CPSR
    msr CPSR_c, r0            @ switch to mode in r0
    mov ip, #0                @ clear ip
    mcr p15, 0, ip, c7, c5, 4 @ prefetch flush
    str sp, [r1]              @ save sp
    str lr, [r2]              @ save lr
    msr CPSR_c, r3            @ restore CPSR
    mov ip, #0                @ clear ip
    mcr p15, 0, ip, c7, c5, 4 @ prefetch flush
    bx lr                     @ return
    @ asm_not_reached();


@ drop in your code from lab 13 [in 1-rfe-asm.S]
@
@ void switchto_user_asm(regs_t *r)  __attribute__((noreturn));
@  - <r> is a 17-entry register entry in r0.
@
@ switch to user mode loading r0-r15, and cpsr from <r>
MK_FN(switchto_user_asm)
    @ b staff_switchto_user_asm
    mov sp, r0                @ set sp to regs pointer
    ldm sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip, sp, lr}^ @ load user regs (^ = user mode)
    add sp, sp, #60           @ adjust sp (15 registers * 4 = 60 bytes)
    rfeia sp                  @ return from exception (loads PC and CPSR)
    asm_not_reached();

@ drop in your code from lab 13
@ 
@ void switchto_priv_asm(regs_t *r)  __attribute__((noreturn));
MK_FN(switchto_priv_asm)
    @ b staff_switchto_priv_asm
    ldr r1, [r0, #64]         @ load CPSR from regs[16] (offset 64 = 16*4)
    msr CPSR_fsxc, r1         @ set CPSR
    mov r2, #0                @ clear r2
    mcr p15, 0, r2, c7, c5, 4 @ prefetch flush
    ldm r0, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip, sp, lr, pc} @ load all regs including PC
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

    mov r3, #0                @ clear r3
    str r3, [r0, #0]          @ r0 = 0 (we don't save caller-saved regs)
    str r3, [r0, #4]          @ r1 = 0
    str r3, [r0, #8]          @ r2 = 0
    str r3, [r0, #12]         @ r3 = 0
    str r3, [r0, #48]         @ ip (r12) = 0
    str r4, [r0, #16]         @ r4
    str r5, [r0, #20]         @ r5
    str r6, [r0, #24]         @ r6
    str r7, [r0, #28]         @ r7
    str r8, [r0, #32]         @ r8
    str r9, [r0, #36]         @ r9
    str sl, [r0, #40]         @ sl (r10)
    str fp, [r0, #44]         @ fp (r11)
    str sp, [r0, #52]         @ sp (r13)
    str lr, [r0, #56]         @ lr (r14)
    str lr, [r0, #60]         @ pc (r15) = lr (return address)
    mrs r3, CPSR              @ get CPSR
    str r3, [r0, #64]         @ save CPSR (r16)

    @ 2. just call the right switch routine: note, we should
    @    *not* reach this location when switching back!
    mov r0, r1                @ move next to r0 (first arg)
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

    @ asm_todo("save the registers: identical to <switchto_priv_asm>")
    @ save the registers: identical to <cswitchto_priv_asm>
    mov r3, #0                @ clear r3
    str r3, [r0, #0]          @ r0 = 0 (we don't save caller-saved regs)
    str r3, [r0, #4]          @ r1 = 0
    str r3, [r0, #8]          @ r2 = 0
    str r3, [r0, #12]         @ r3 = 0
    str r3, [r0, #48]         @ ip (r12) = 0
    str r4, [r0, #16]         @ r4
    str r5, [r0, #20]         @ r5
    str r6, [r0, #24]         @ r6
    str r7, [r0, #28]         @ r7
    str r8, [r0, #32]         @ r8
    str r9, [r0, #36]         @ r9
    str sl, [r0, #40]         @ sl (r10)
    str fp, [r0, #44]         @ fp (r11)
    str sp, [r0, #52]         @ sp (r13)
    str lr, [r0, #56]         @ lr (r14)
    str lr, [r0, #60]         @ pc (r15) = lr (return address)
    mrs r3, CPSR              @ get CPSR
    str r3, [r0, #64]         @ save CPSR (r16)

    mov r0, r1                @ move next to r0 (first arg)
    bl switchto_user_asm
    asm_not_reached();

@ cswitch, saving current registers in <old> and
@ loading the registers from <next>, jump to <fn>
@ when done.
@
@ void cswitch_asm(regs_t *old, regs_t *next, switch_fn_t fn);
@   - old in r0
@   - next in r1
@   - fn in r2
MK_FN(cswitch_asm)
    mov r3, #0                @ clear r3
    str r3, [r0, #0]          @ r0 = 0 (we don't save caller-saved regs)
    str r3, [r0, #4]          @ r1 = 0
    str r3, [r0, #8]          @ r2 = 0
    str r3, [r0, #12]         @ r3 = 0
    str r3, [r0, #48]         @ ip (r12) = 0
    str r4, [r0, #16]         @ r4
    str r5, [r0, #20]         @ r5
    str r6, [r0, #24]         @ r6
    str r7, [r0, #28]         @ r7
    str r8, [r0, #32]         @ r8
    str r9, [r0, #36]         @ r9
    str sl, [r0, #40]         @ sl (r10)
    str fp, [r0, #44]         @ fp (r11)
    str sp, [r0, #52]         @ sp (r13)
    str lr, [r0, #56]         @ lr (r14)
    str lr, [r0, #60]         @ pc (r15) = lr (return address)
    mrs r3, CPSR              @ get CPSR
    str r3, [r0, #64]         @ save CPSR (r16)
    mov r0, r1                @ move next to r0 (first arg)
    blx r2                    @ call fn(next)
    asm_not_reached();

@ return the original level?
@
@ uint32_t switchto_user(void);
@   - switches to user mode, preserving sp and lr
@   - returns the original CPSR
@ MK_FN(switchto_user)
@     mrs r0, CPSR              @ save CPSR to r0 (return value)
@     mov r1, sp                @ save sp to r1
@     mov r2, lr                @ save lr to r2
@     cps #16                   @ switch to user mode (mode 16)
@     mov r3, #0                @ clear r3
@     mcr p15, 0, r3, c7, c5, 4 @ prefetch flush
@     mov sp, r1                @ restore sp
@     mov lr, r2                @ restore lr
@     bx lr                     @ return (CPSR in r0)
