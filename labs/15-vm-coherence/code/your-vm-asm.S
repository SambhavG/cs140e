@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ many page numbers are in 
@    armv6-coprocessor-asm.h
@ and the other armv6-*.h files

MK_FN(cp15_ctrl_reg1_rd)
    CONTROL_REG1_RD(r0)
    bx lr

MK_FN(cp15_ctrl_reg1_wr)
    CONTROL_REG1_WR(r0)
    mov r1, #0
    PREFETCH_FLUSH(r1)
    bx lr

// just read the cache type: use C code to figure out if it is unified
// b6-13, b6-14
MK_FN(get_cache_type)
    CACHE_TYPE_RD(r0)
    bx lr

// void reset(void)
// clear the dcache, icache, tlbs
MK_FN(mmu_reset)
    @ b staff_mmu_reset
    mov r0, #0 @ so the caches get reset
    INV_ALL_CACHES(r0) @ see instructions
    INV_TLB(r0) @ see instructions
    FLUSH_BTB(r0) @ often used with the other two below
    DSB(r0) @ When we invalidate icache, must dsb
    PREFETCH_FLUSH(r0) @to finish all the coproc writes
    bx lr

// void cp15_domain_ctrl_wr(uint32_t dom_reg);
//  need to do cp15 sync: flush_btb, dsb, prefetch flush
MK_FN(cp15_domain_ctrl_wr)
    @ b staff_cp15_domain_ctrl_wr
    CONTROL_REG1_WR(r0)
    mov r0, #0
    FLUSH_BTB(r0) @ instructions
    DSB(r0) @ instructions
    PREFETCH_FLUSH(r0) @ wrote to coproc, must prefetch flush
    bx lr

// void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
// sequence from b2-25
MK_FN(cp15_set_procid_ttbr0)
    @ b staff_cp15_set_procid_ttbr0

    @ Directly following b2-25
    mov r2, #0 @ use r2 as flushing reg
    DSB(r2)
    ASID_SET(r2)
    PREFETCH_FLUSH(r2)
    TTBR_BASE_CTRL_WR(r2)
    PREFETCH_FLUSH(r2)
    TTBR0_SET(r1)
    TTBR1_SET(r2)
    PREFETCH_FLUSH(r2)
    DSB(r2)
    ASID_SET(r0)

    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

// void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_disable_set_asm)
    @ b staff_mmu_disable_set_asm

    @ note: this piece of code is mapped into the sample place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.

    @ Reason for cleaninvdcache -> set -> inv icache:
    @ Must make sure everything in dcache is written out before we turn off mmu
    @ Must make sure we don't have old instructions left over from pre-"mmu turned off" context


    mov r2, #0 @ using r2 as our zero reg for clearing

    @ Clean and invalidate dcache
    CLEAN_INV_DCACHE(r2)

    @ Flush everything
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    @ Turn off MMU (assuming user's reg turns it off)
    CONTROL_REG1_WR(r0)

    @ Flush everything (may only need prefetch since only writing to coproc, not invalidating anything)
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    @ Invalidate icache
    INV_ICACHE(r2)

    @ Flush everything
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

// void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_enable_set_asm)
    @ b staff_mmu_enable_set_asm
    @ high level on 6-9
    @ Get the current value of the control register, turn off 

    CONTROL_REG1_RD(r1) @ cur value of control reg
    mov r2, #0 @ using r2 as our zero reg for clearing

    @ turn off the I bit (instruction cache) before invalidating it
    BIC r1, #0b1000000000000
    CONTROL_REG1_WR(r1)

    @ flush everything
    @ FLUSH_BTB(r2)
    @ DSB(r2)
    PREFETCH_FLUSH(r2)

    @ disable and invalidate instruction cache
    INV_ICACHE(r2)

    @flush everything
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    @ write the user's original value in r0
    CONTROL_REG1_WR(r0)

    @ flush everything
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    bx lr


@ STR rx, [Page table entry] ;
@ Clean line [Page table entry]
@ DSB ; ensures visibility of the data cleaned from the D Cache
@ Invalidate TLB entry by MVA [page address]
@ Invalidate BTB
@ DSB ; ensure completion of the Invalidate TLB
@ PrefetchFlush

MK_FN(mmu_sync_pte_mods)
    @ b staff_mmu_sync_pte_mods

    mov r2, #0
    CLEAN_INV_DCACHE(r2)
    @Also do icache according to instructions
    INV_ICACHE(r2)

    DSB(r2)
    INV_TLB(r2)
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)


    @flush everything
    FLUSH_BTB(r2)
    DSB(r2)
    PREFETCH_FLUSH(r2)

    bx lr

