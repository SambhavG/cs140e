#include "rpi-asm.h"

MK_FN(syscall_hello)
    @ we are already at system level: running this will trash
    @ the lr, so we need to save it.
    push {lr}
    swi 1
    pop {lr}
    bx lr

@ your code should reject this call with an error.
MK_FN(syscall_illegal)
    push {lr}
    swi 2
    pop {lr}
    bx lr

@ void run_user_code_asm(uint32_t pc, uint32_t stack)
@
@ utility routine:
@   1. switch to user mode, 
@   2. set setting <sp> to <stack>
@   3. jump to the address in <pc> 
@ 
@ recall:
@   - pc is passed in r0
@   - stack passed in r1
@
@ look at notes/mode-bugs for examples on what to do
@    - change modes using the cps instruction.
@
@ does not return
MK_FN(run_user_code_asm)
    @ @ user mode
    @ @ get the current mode
    @ mrs r2, cpsr
    @ @ clear the bottom 5 bits of r2
    @ bic r2, r2, #(1<<5 - 1)
    @ @ set the mode to user
    @ orr r2, r2, #USER_MODE
    @ @ write back to cspr
    @ msr cpsr, r2

    @ user mode
    cps #USER_MODE

    @ flush
    prefetch_flush(r2)

    @ set sp to the stack pointer
    mov sp, r1
    @ jump to pc
    bx r0
