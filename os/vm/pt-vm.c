#include "pt-vm.h"
#include "helper-macros.h"
#include "libc/bit-support.h"
#include "procmap.h"
#include "pt-vm.h"
#include "rpi.h"

// turn this off if you don't want all the debug output.
enum { verbose_p = 0 };
enum { OneMB = 1024 * 1024 };
enum { FourKB = 4096 };

static vm_pt_t *kernel_pt = 0;

// Simple 4KB page allocator - allocates from a pool of physical memory
// Start after kernel heap (at 32MB)
static uint32_t page_pool_start = 0;
static uint32_t page_pool_ptr = 0;
static uint32_t page_pool_end = 0;

// Initialize the 4KB page allocator
// Should be called after kmalloc_init, uses memory after the kernel heap
void vm_page_alloc_init(uint32_t start_mb, uint32_t size_mb) {
  page_pool_start = start_mb * OneMB;
  page_pool_ptr = page_pool_start;
  page_pool_end = page_pool_start + size_mb * OneMB;
  if (verbose_p)
    output("Page allocator initialized: 0x%x - 0x%x (%d MB)\n", page_pool_start,
           page_pool_end, size_mb);
}

// Allocate a 4KB physical page
uint32_t vm_page_alloc(void) {
  if (page_pool_ptr >= page_pool_end)
    panic("vm_page_alloc: out of physical pages!\n");

  uint32_t pa = page_pool_ptr;
  page_pool_ptr += FourKB;

  // Zero the page
  memset((void *)pa, 0, FourKB);

  return pa;
}

// Allocate n contiguous 4KB physical pages
uint32_t vm_pages_alloc(uint32_t n) {
  if (page_pool_ptr + n * FourKB > page_pool_end)
    panic("vm_pages_alloc: out of physical pages!\n");

  uint32_t pa = page_pool_ptr;
  page_pool_ptr += n * FourKB;

  // Zero the pages
  memset((void *)pa, 0, n * FourKB);

  return pa;
}

vm_pt_t *vm_pt_alloc(unsigned n) {
  // demand(n == 4096, we only handling a fully - populated page table right
  // now);

  vm_pt_t *pt = 0;
  unsigned nbytes = n * sizeof *pt;

  // trivial:
  // allocate pt with n entries [should look just like you did
  // for pinned vm]
  // pt = staff_vm_pt_alloc(n);
  pt = kmalloc_aligned(nbytes, 1 << 14);

  demand(is_aligned_ptr(pt, 1 << 14), must be 14 - bit aligned !);
  return pt;
}

// allocate new page table and copy pt.  not the
// best interface since it will copy private mappings.
vm_pt_t *vm_dup(vm_pt_t *pt1) {
  vm_pt_t *pt2 = vm_pt_alloc(PT_LEVEL1_N);
  memcpy(pt2, pt1, PT_LEVEL1_N * sizeof *pt1);
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
vm_pte_t *vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr) {
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

  if (verbose_p)
    vm_pte_print(pt, pte);
  assert(pte);
  return pte;
}

// lookup 32-bit address va in pt and return the pte
// if it exists, 0 otherwise.
vm_pte_t *vm_lookup(vm_pt_t *pt, uint32_t va) {
  // Get top 12 bits
  if (pt[va >> 20].tag == 0b10) {
    return &pt[va >> 20];
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

  vm_pt_t *page = vm_lookup(pt, va);
  if (page == 0)
    return 0;

  *pa = (page->sec_base_addr << 20) | (va & 0xfffff);
  return page;

  // return staff_vm_xlate(pa,pt,va);
}

// compute the default attribute for each type.
static inline pin_t attr_mk(pr_ent_t *e) {
  switch (e->type) {
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
// if <enable_p>=1 will enable the MMU.  make sure
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
    // asdf
    pin_t attr = attr_mk(&(p->map[i]));
    vm_map_sec(pt, p->map[i].addr, p->map[i].addr, attr);

    assert(vm_lookup(pt, p->map[i].addr) != 0);
  }
  mmu_set_ctx(kern_pid, kern_asid, pt);

  if (enable_p >= 1)
    vm_mmu_enable();

  // Store kernel page table globally
  kernel_pt = pt;

  assert(pt);
  return pt;
}

void vm_set_kernel_pt(vm_pt_t *pt) { kernel_pt = pt; }

vm_pt_t *vm_get_kernel_pt(void) {
  assert(kernel_pt);
  return kernel_pt;
}

// Map a 4KB page at virtual address <va> to physical address <pa>
// Uses coarse page table (L1 -> L2 -> 4KB page)
void vm_map_page(vm_pt_t *l1_pt, uint32_t va, uint32_t pa, mem_perm_t perm,
                 mem_attr_t attr, uint32_t dom, int global_p) {
  assert(va % FourKB == 0);
  assert(pa % FourKB == 0);

  // First level index: bits [31:20]
  uint32_t l1_idx = va >> 20;
  assert(l1_idx < PT_LEVEL1_N);

  // Cast L1 entry to coarse page table descriptor
  coarse_pt_desc_t *l1_desc = (coarse_pt_desc_t *)&l1_pt[l1_idx];

  // If first-level entry doesn't point to a coarse page table, create one
  // tag == 0b01 means coarse page table
  if (l1_desc->tag != 0b01) {
    // Allocate aligned coarse page table (1KB = 256 entries * 4 bytes)
    // Must be 1KB aligned (10 bits)
    small_page_desc_t *l2_pt = kmalloc_aligned(1024, 1024);
    memset(l2_pt, 0, 1024);
    assert(((uint32_t)l2_pt & 0x3FF) == 0); // 1KB aligned

    // Setup first-level descriptor as coarse page table pointer
    // Clear all bits first
    *(uint32_t *)l1_desc = 0;
    l1_desc->tag = 0b01;   // coarse page table
    l1_desc->sbz0 = 0;     // should be zero
    l1_desc->domain = dom; // domain
    l1_desc->IMP = 0;      // implementation defined = 0
    l1_desc->pt_base_addr = ((uint32_t)l2_pt) >> 10;

    if (verbose_p)
      output("Created L2 PT at L1 idx=%d, L2 PT=%p, descriptor=0x%x\n", l1_idx,
             l2_pt, *(uint32_t *)l1_desc);
  }

  // Get second-level table base address
  small_page_desc_t *l2_base =
      (small_page_desc_t *)(l1_desc->pt_base_addr << 10);

  // Second level index: bits [19:12]
  uint32_t l2_idx = (va >> 12) & 0xFF;
  assert(l2_idx < PT_LEVEL2_N);

  small_page_desc_t *l2_entry = &l2_base[l2_idx];

  // Setup small page descriptor
  // Clear all bits first
  *(uint32_t *)l2_entry = 0;

  // Small page: bit 1 = 1, bit 0 = XN
  l2_entry->XN = 0;   // executable (XN=0)
  l2_entry->tag1 = 1; // bit 1 = 1 for small page
  l2_entry->B = attr & 0b1;
  l2_entry->C = (attr >> 1) & 0b1;
  l2_entry->AP0 = perm & 0b11;
  l2_entry->TEX = (attr >> 2) & 0b111;
  l2_entry->AP2 = (perm >> 2) & 0b1;
  l2_entry->S = 0; // non-shareable
  l2_entry->nG =
      global_p ? 0 : 1; // nG=0 means global, nG=1 means process-specific
  l2_entry->base_addr = pa >> 12;

  if (verbose_p)
    output("Mapped 4KB page: VA=0x%x -> PA=0x%x, L2[%d]=0x%x\n", va, pa, l2_idx,
           *(uint32_t *)l2_entry);
}

// Free a page table and all its second-level tables
// Note: kmalloc doesn't support free(), so this is a no-op for now
void vm_pt_free(vm_pt_t *pt) {
  (void)pt; // suppress unused warning
  // With a bump allocator that never frees, we can't actually reclaim memory.
  // In a real system, we would walk all L1 entries and free L2 tables.
}

// Create a user page table that inherits kernel mappings
// The kernel mappings use 1MB sections, user mappings will use 4KB pages
vm_pt_t *vm_user_pt_create(void) {
  // Allocate a new L1 page table
  vm_pt_t *pt = vm_pt_alloc(PT_LEVEL1_N);

  // Copy kernel mappings (these are section mappings)
  // We only copy entries that have tag == 0b10 (sections)
  assert(kernel_pt);
  for (uint32_t i = 0; i < PT_LEVEL1_N; i++) {
    if (kernel_pt[i].tag == 0b10) {
      // This is a section mapping, copy it
      pt[i] = kernel_pt[i];
    }
  }

  if (verbose_p)
    output("Created user page table at %p, copied kernel sections\n", pt);

  return pt;
}

// Map a contiguous range of virtual addresses to physical addresses
// using 4KB pages
void vm_map_pages(vm_pt_t *pt, uint32_t va_start, uint32_t pa_start,
                  uint32_t nbytes, mem_perm_t perm, mem_attr_t attr,
                  uint32_t dom, int global_p) {
  // Round up to page boundary
  uint32_t npages = (nbytes + FourKB - 1) / FourKB;
  printk("\n[vm_map_pages] mapping %d pages from VA 0x%x to PA 0x%x\n", npages,
         va_start, pa_start);
  for (uint32_t i = 0; i < npages; i++) {
    uint32_t va = va_start + i * FourKB;
    uint32_t pa = pa_start + i * FourKB;
    vm_map_page(pt, va, pa, perm, attr, dom, global_p);
  }

  if (verbose_p)
    output("Mapped %d pages: VA 0x%x-0x%x -> PA 0x%x-0x%x\n", npages, va_start,
           va_start + npages * FourKB, pa_start, pa_start + npages * FourKB);
}

// Look up the physical address for a virtual address in a page table
// Returns the physical address of the first 4KB page at va_start
// Assumes the mapping exists and is a 4KB page (coarse page table)
uint32_t vm_get_pa_from_pt(vm_pt_t *pt, uint32_t va) {
  assert(va % FourKB == 0);

  // First level index: bits [31:20]
  uint32_t l1_idx = va >> 20;
  assert(l1_idx < PT_LEVEL1_N);

  // Check if this is a section mapping (1MB)
  if (pt[l1_idx].tag == 0b10) {
    // Section mapping: PA = section base + offset within section
    return (pt[l1_idx].sec_base_addr << 20) | (va & 0xFFFFF);
  }

  // Must be a coarse page table (tag == 0b01)
  coarse_pt_desc_t *l1_desc = (coarse_pt_desc_t *)&pt[l1_idx];
  if (l1_desc->tag != 0b01) {
    panic("vm_get_pa_from_pt: VA 0x%x not mapped (L1 tag=%d)\n", va,
          l1_desc->tag);
  }

  // Get second-level table base address
  small_page_desc_t *l2_base =
      (small_page_desc_t *)(l1_desc->pt_base_addr << 10);

  // Second level index: bits [19:12]
  uint32_t l2_idx = (va >> 12) & 0xFF;
  small_page_desc_t *l2_entry = &l2_base[l2_idx];

  // Check that it's a valid small page (tag1 == 1)
  if (l2_entry->tag1 != 1) {
    panic("vm_get_pa_from_pt: VA 0x%x not mapped (L2 tag1=%d)\n", va,
          l2_entry->tag1);
  }

  // Return physical address
  return l2_entry->base_addr << 12;
}
