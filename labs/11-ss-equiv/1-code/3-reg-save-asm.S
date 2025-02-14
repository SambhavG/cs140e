#include "rpi-asm.h"

@ save all the general registers and the spsr in 
@ ascending order onto a block of memory on the
@ stack.  the pointer to the base of this to 
@ do_syscall (defined in `2-reg-save.c` and 
@ `2-reg-save-full.c`).
@
swi_trampoline:
    stm r0, {r0-r14, pc}^
    add r0, #60
    ldr r1, =swi_fn
    add r1, #4
    stm r0, {r1}
    sub r0, #60


    mrs r1, spsr
    add r0, #64
    stm r0, {r1}
    sub r0, #64

    mov sp, #INT_STACK_ADDR
    bl do_syscall

    @ note: this trampoline does not resume.
    asm_not_reached();

@ only handle single step.
.align 5; 
.globl swi_test_handlers
swi_test_handlers:
    b unhandled_reset
    b unhandled_undefined_instruction
    b swi_trampoline
    b unhandled_prefetch_abort 
    b unhandled_data_abort
    b unhandled_interrupt
    b unhandled_fiq


.weak do_syscall
do_syscall:
    asm_not_reached();
