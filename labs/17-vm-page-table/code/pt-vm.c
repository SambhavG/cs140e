#include "rpi.h"
#include "pt-vm.h"
#include "helper-macros.h"
#include "procmap.h"
#include "pt-vm.h"

// turn this off if you don't want all the debug output.
enum { verbose_p = 1 };
enum { OneMB = 1024*1024 };

vm_pt_t *vm_pt_alloc(unsigned n) {
    demand(n == 4096, we only handling a fully-populated page table right now);

    vm_pt_t *pt = 0;
    unsigned nbytes = n * sizeof *pt;

    // trivial:
    // allocate pt with n entries [should look just like you did 
    // for pinned vm]
    // pt = staff_vm_pt_alloc(n);
    pt = kmalloc_aligned(nbytes, 1<<14);

    demand(is_aligned_ptr(pt, 1<<14), must be 14-bit aligned!);
    return pt;
}

// allocate new page table and copy pt.  not the
// best interface since it will copy private mappings.
vm_pt_t *vm_dup(vm_pt_t *pt1) {
    vm_pt_t *pt2 = vm_pt_alloc(PT_LEVEL1_N);
    memcpy(pt2,pt1,PT_LEVEL1_N * sizeof *pt1);
    return pt2;
}

// same as pinned version: 
//  - probably should check that the page table 
//    is set, and asid makes sense.
void vm_mmu_enable(void) {
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
}

// same as pinned 
void vm_mmu_disable(void) {
    assert(mmu_is_enabled());
    mmu_disable();
    assert(!mmu_is_enabled());
}

// - set <pt,pid,asid> for an address space.
// - must be done before you switch into it!
// - mmu can be off or on.
void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid) {
    assert(pt);
    mmu_set_ctx(pid, asid, pt);
}

// just like pinned.
void vm_mmu_init(uint32_t domain_reg) {
    // initialize everything, after bootup.
    mmu_init();
    domain_access_ctrl_set(domain_reg);
}

// map the 1mb section starting at <va> to <pa>
// with memory attribute <attr>.
vm_pte_t *
vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr) 
{
    assert(aligned(va, OneMB));
    assert(aligned(pa, OneMB));

    // today we just use 1mb.
    assert(attr.pagesize == PAGE_1MB);

    unsigned index = va >> 20;
    assert(index < PT_LEVEL1_N);

    vm_pte_t *pte = 0;
    // return staff_vm_map_sec(pt,va,pa,attr);


    pte = &pt[index];
    pte->tag = 0b10;
    pte->B = attr.mem_attr & 0b1;
    pte->C = (attr.mem_attr & 0b10) >> 1;
    pte->XN = 0;
    pte->domain = attr.dom;
    pte->IMP = 0;
    pte->AP = attr.AP_perm & 0b11;
    pte->TEX = (attr.mem_attr & 0b11100) >> 2;
    pte->APX = (attr.AP_perm & 0b100) >> 2;
    pte->S = 0;
    pte->nG = !(attr.G);
    pte->super = 0;
    pte->sec_base_addr = pa >> 20;


    if(verbose_p)
        vm_pte_print(pt,pte);
    assert(pte);
    return pte;
}

// lookup 32-bit address va in pt and return the pte
// if it exists, 0 otherwise.
vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va) {
    //Get top 12 bits
    if (pt[va>>20].tag == 0b10) {
        return &pt[va>>20];
    }
    return 0;

    // return staff_vm_lookup(pt,va);
}

// manually translate <va> in page table <pt>
// - if doesn't exist, returns 0.
// - if does exist returns the corresponding physical
//   address in <pa>
//
// NOTE: 
//   - we can't just return the <pa> b/c page 0 could be mapped.
//   - the common unix kernel hack of returning (void*)-1 leads
//     to really really nasty bugs.  so we don't.
vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va) {
    
    vm_pt_t* page = vm_lookup(pt, va);
    if (page == 0) return 0;

    *pa = (page->sec_base_addr<<20) | (va & 0xfffff);
    return page;

    // return staff_vm_xlate(pa,pt,va);
}

// compute the default attribute for each type.
static inline pin_t attr_mk(pr_ent_t *e) {
    switch(e->type) {
    case MEM_DEVICE: 
        return pin_mk_device(e->dom);
    // kernel: currently everything is uncached.
    case MEM_RW:
        return pin_mk_global(e->dom, perm_rw_priv, MEM_uncached);
   case MEM_RO: 
        panic("not handling\n");
   default: 
        panic("unknown type: %d\n", e->type);
    }
}

// setup the initial kernel mapping.  This will mirror
//  static inline void procmap_pin_on(procmap_t *p) 
// in <13-pinned-vm/code/procmap.h>  but will call
// your vm_ routines, not pinned routines.
//
// if <enable_p>=1, should enable the MMU.  make sure
// you setup the page table and asid. use  
// kern_asid, and kern_pid.
vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p) {
    // the asid and pid we start with.  
    //    shouldn't matter since kernel is global.
    enum { kern_asid = 1, kern_pid = 0x140e };

    vm_pt_t *pt = 0;

    // return staff_vm_map_kernel(p,enable_p);
    uint32_t d = dom_perm(p->dom_ids, DOM_client);
    vm_mmu_init(d);
    pt = vm_pt_alloc(PT_LEVEL1_N);

    for (int i = 0; i < p->n; i++) {
        //asdf
       pin_t attr = attr_mk(&(p->map[i]));
       vm_map_sec(pt, p->map[i].addr, p->map[i].addr , attr);

       assert(vm_lookup(pt, p->map[i].addr) != 0);
    }
    mmu_set_ctx(kern_pid, kern_asid, pt);

    if (enable_p >= 1) vm_mmu_enable();
    

    assert(pt);
    return pt;
}
