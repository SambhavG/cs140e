// put your code here.
//
#include "pinned-vm.h"

// generate the _get and _set methods.
// (see asm-helpers.h for the cp_asm macro
// definition)
// arm1176.pdf: 3-149

cp_asm(lockdown_index, p15, 5, c15, c4, 2);
cp_asm(lockdown_va, p15, 5, c15, c5, 2);
cp_asm(lockdown_pa, p15, 5, c15, c6, 2);
cp_asm(lockdown_attr, p15, 5, c15, c7, 2);
cp_asm_get(xlate_pa, p15, 0, c7, c4, 0);
cp_asm_set(xlate_kern_rd, p15, 0, c7, c8, 0);
cp_asm_set(xlate_kern_wr, p15, 0, c7, c8, 1);
cp_asm_set(dacr, p15, 0, c3, c0, 0);
static void *null_pt = 0;

// fill this in based on the <1-test-basic-tutorial.c>
// NOTE:
//    you'll need to allocate an invalid page table
void pin_mmu_init(uint32_t domain_reg) {
  null_pt = kmalloc_aligned(4096 * 4, 1 << 14);
  assert((uint32_t)null_pt % (1 << 14) == 0);
  assert(!mmu_is_enabled());
  domain_access_ctrl_set(domain_reg);
  mmu_init();

  // staff_pin_mmu_init(domain_reg);
  return;
}

// do a manual translation in tlb:
//   1. store result in <result>
//   2. return 1 if entry exists, 0 otherwise.
//
// NOTE: mmu must be on (confusing).
int tlb_contains_va(uint32_t *result, uint32_t va) {
  assert(mmu_is_enabled());

  // 3-79
  assert(bits_get(va, 0, 2) == 0);

  xlate_kern_rd_set(va);
  uint32_t res = xlate_pa_get();

  *result = (res >> 10) << 10;
  *result = bits_set(*result, 0, 9, bits_get(va, 0, 9));

  return !bit_isset(res, 0);

  // return staff_tlb_contains_va(result, va);
}

// map <va>-><pa> at TLB index <idx> with attributes <e>
void pin_mmu_sec(unsigned idx, uint32_t va, uint32_t pa, pin_t e) {

  demand(idx < 8, lockdown index too large);
  // lower 20 bits should be 0.
  demand(bits_get(va, 0, 19) == 0, only handling 1MB sections);
  demand(bits_get(pa, 0, 19) == 0, only handling 1MB sections);

  debug("about to map %x->%x\n", va, pa);

  // these will hold the values you assign for the tlb entries.
  uint32_t x, va_ent = 0, pa_ent = 0, attr = 0;

  x = idx;
  lockdown_index_set(x);

  // set va, G, and asid
  va_ent = va;
  va_ent = bits_set(va_ent, 9, 9, e.G);
  va_ent = bits_set(va_ent, 0, 7, e.asid);
  lockdown_va_set(va_ent);

  // set PA
  pa_ent = pa; // already know bottom bits are 0
  // set apx, ap
  pa_ent = bits_set(pa_ent, 1, 3, e.AP_perm);
  // set pagesize
  pa_ent = bits_set(pa_ent, 6, 7, e.pagesize);
  // set v
  pa_ent = bits_set(pa_ent, 0, 0, 1);
  // not setting nsa, nstid
  lockdown_pa_set(pa_ent);

  // set attr
  // tex, c, b
  attr = bits_set(attr, 1, 5, e.mem_attr);
  // dom
  attr = bits_set(attr, 7, 10, e.dom);
  lockdown_attr_set(attr);

  if ((x = lockdown_va_get()) != va_ent)
    panic("lockdown va: expected %x, have %x\n", va_ent, x);
  if ((x = lockdown_pa_get()) != pa_ent)
    panic("lockdown pa: expected %x, have %x\n", pa_ent, x);
  if ((x = lockdown_attr_get()) != attr)
    panic("lockdown attr: expected %x, have %x\n", attr, x);
}

// check that <va> is pinned.
int pin_exists(uint32_t va, int verbose_p) {
  if (!mmu_is_enabled())
    panic("XXX: i think we can only check existence w/ mmu enabled\n");

  uint32_t r;
  if (tlb_contains_va(&r, va)) {
    assert(va == r);
    return 1;
  } else {
    if (verbose_p)
      output("TLB should have %x: returned %x [reason=%b]\n", va, r,
             bits_get(r, 1, 6));
    return 0;
  }
}

// look in test <1-test-basic.c> to see what to do.
// need to set the <asid> before turning VM on and
// to switch processes.
void pin_set_context(uint32_t asid) {
  // put these back
  demand(asid > 0 && asid < 64, invalid asid);
  demand(null_pt, must setup null_pt-- - look at tests);
  mmu_set_ctx(128, asid, null_pt);
  // staff_pin_set_context(asid);
}

void pin_clear(unsigned idx) {
  demand(idx <= 7, invalid idx);

  lockdown_index_set(idx);
  lockdown_va_set(0);
  lockdown_pa_set(0);
  lockdown_attr_set(0);

  assert(lockdown_va_get() == 0);
  assert(lockdown_pa_get() == 0);
  assert(lockdown_attr_get() == 0);

  // staff_pin_clear(idx);
}

void staff_lockdown_print_entry(unsigned idx);

void lockdown_print_entry(unsigned idx) {

  trace("   idx=%d\n", idx);
  lockdown_index_set(idx);
  uint32_t va_ent = lockdown_va_get();
  uint32_t pa_ent = lockdown_pa_get();
  uint32_t attr = lockdown_attr_get();
  unsigned v = bit_get(pa_ent, 0);

  if (!v) {
    trace("     [invalid entry %d]\n", idx);
    return;
  }

  uint32_t va = (va_ent >> 12);
  uint32_t pa = (pa_ent >> 12);
  uint32_t G = bits_get(va_ent, 9, 9);
  uint32_t asid = bits_get(va_ent, 0, 7);
  uint32_t AP_perm = bits_get(pa_ent, 1, 3);
  uint32_t pagesize = bits_get(pa_ent, 6, 7);
  uint32_t nsa = bits_get(pa_ent, 9, 9);
  uint32_t nstid = bits_get(pa_ent, 8, 8);
  uint32_t apx = bits_get(pa_ent, 1, 3);
  uint32_t ap = bits_get(pa_ent, 1, 2);
  uint32_t size = bits_get(pa_ent, 6, 7);
  uint32_t dom = bits_get(attr, 7, 10);
  uint32_t xn = bits_get(attr, 6, 6);
  uint32_t tex = bits_get(attr, 3, 5);
  uint32_t C = bits_get(attr, 2, 2);
  uint32_t B = bits_get(attr, 1, 1);

  // 3-149
  trace("     va_ent=%x: va=%x|G=%d|ASID=%d\n", va_ent, va, G, asid);

  // 3-150
  trace("     pa_ent=%x: pa=%x|nsa=%d|nstid=%d|size=%b|apx=%b|v=%d\n", pa_ent,
        pa, nsa, nstid, size, apx, v);

  // 3-151
  trace("     attr=%x: dom=%d|xn=%d|tex=%b|C=%d|B=%d\n", attr, dom, xn, tex, C,
        B);
}

void lockdown_print_entries(const char *msg) {
  trace("-----  <%s> ----- \n", msg);
  trace("  pinned TLB lockdown entries:\n");
  for (int i = 0; i < 8; i++)
    lockdown_print_entry(i);
  trace("----- ---------------------------------- \n");
}