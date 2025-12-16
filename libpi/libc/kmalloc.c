// kmalloc.c - reverse-engineered from kmalloc.o (ARM32 little-endian)

#include <stddef.h>
#include <stdint.h>

extern void *memset(void *s, int c, size_t n);

// These are referenced by the object (stubs/externs in your kernel):
extern uintptr_t program_end(void);
extern void printk(const char *fmt, ...);
extern void clean_reboot(void);

// Backing state layout inferred from loads/stores at offsets 0,4,8.
typedef struct {
  uintptr_t end;   // +0
  uintptr_t start; // +4
  uintptr_t ptr;   // +8
} kmalloc_heap_t;

static kmalloc_heap_t
    g_heap; // this is what the .word 0 placeholders resolve to via relocations

// Accessors (match kmalloc_heap_end/start/ptr functions)
uintptr_t kmalloc_heap_end(void) { return g_heap.end; }
uintptr_t kmalloc_heap_start(void) { return g_heap.start; }
uintptr_t kmalloc_heap_ptr(void) { return g_heap.ptr; }

// Sets heap start and size, and resets ptr to start.
// Validations match the branches in kmalloc_init_set_start.
void kmalloc_init_set_start(uintptr_t start, uintptr_t size) {
  uintptr_t pend = program_end();

  // if (start < program_end()) -> error + reboot
  if (start < pend) {
    printk("kmalloc_init_set_start:%d: start < program_end\n", 0x30);
    clean_reboot();
    __builtin_unreachable();
  }

  // if (size == 0) -> error + reboot
  if (size == 0) {
    printk("kmalloc_init_set_start:%d: size == 0\n", 0x32);
    clean_reboot();
    __builtin_unreachable();
  }

  // if (g_heap.start > start) -> error + reboot
  if (g_heap.start > start) {
    printk(
        "kmalloc_init_set_start:%d: new start precedes existing heap start\n",
        0x34);
    clean_reboot();
    __builtin_unreachable();
  }

  // if (g_heap.ptr > start) -> error + reboot (prints both start and current
  // end in the .o)
  if (g_heap.ptr > start) {
    printk("kmalloc_init_set_start:%d: new start (%p) precedes current heap "
           "end/ptr (%p)\n",
           0x38, (void *)start, (void *)g_heap.end);
    clean_reboot();
    __builtin_unreachable();
  }

  g_heap.ptr = start;
  g_heap.start = start;
  g_heap.end = start + size;
}

// Internal allocator for size != 0. Returns old heap ptr, advances by size
// rounded up to 8.
static void *kmalloc_notzero(size_t size) {
  // printk("kmalloc requested for size %d, remaining capacity is %d\n", size,
  //  g_heap.end - g_heap.ptr);
  // if (size == 0) -> error + reboot
  if (size == 0) {
    printk("kmalloc_notzero:%d: size == 0\n", 0x41);
    clean_reboot();
    __builtin_unreachable();
  }

  // round up to 8: (size + 7) & ~7
  size_t rounded = (size + 7u) & ~((size_t)7u);

  uintptr_t p = g_heap.ptr;

  // if (p & 7) -> error + reboot
  if ((p & 7u) != 0) {
    printk("kmalloc_notzero:%d: heap ptr not 8-byte aligned\n", 0x44);
    clean_reboot();
    __builtin_unreachable();
  }

  // if (p == 0) -> error + reboot (means not initialized)
  if (p == 0) {
    printk("kmalloc_notzero:%d: heap not initialized\n", 0x48);
    clean_reboot();
    __builtin_unreachable();
  }

  uintptr_t newp = p + (uintptr_t)rounded;
  g_heap.ptr = newp;

  // if (newp > end) -> error + reboot; code computes (newp - end) for printing
  if (newp > g_heap.end) {
    uintptr_t over = newp - g_heap.end;
    printk("kmalloc_notzero:%d: out of heap (need %u bytes, over by %p)\n",
           0x4c, (unsigned)rounded, (void *)over);
    clean_reboot();
    __builtin_unreachable();
  }

  return (void *)p;
}

// Public kmalloc: size must be nonzero; allocates and zeroes exactly `size`
// bytes.
void *kmalloc(size_t size) {
  if (size == 0) {
    printk("kmalloc:%d: size == 0\n", 0x54);
    clean_reboot();
    __builtin_unreachable();
  }

  void *p = kmalloc_notzero(size);
  memset(p, 0, size);
  return p;
}

// Aligned allocation: aligns the heap pointer up to `align` (min 8), then
// kmalloc(size).
void *kmalloc_aligned(size_t size, size_t align) {
  if (size == 0) {
    printk("kmalloc_aligned:%d: size == 0\n", 0x5e);
    clean_reboot();
    __builtin_unreachable();
  }

  // Check power-of-two: (align & -align) == align
  // (the asm uses rsb/bics to detect non power-of-two)
  if (align == 0 || (align & (0u - align)) != align) {
    printk("kmalloc_aligned:%d: align not power-of-two\n", 0x5f);
    clean_reboot();
    __builtin_unreachable();
  }

  if (align < 8)
    align = 8;

  // (asm re-checks power-of-two after clamp; keep same behavior)
  if ((align & (0u - align)) != align) {
    printk("kmalloc_aligned:%d: internal align check failed\n", 0x11);
    clean_reboot();
    __builtin_unreachable();
  }

  uintptr_t p = g_heap.ptr;

  // Align up: (p + align - 1) & ~(align - 1)
  uintptr_t aligned = (p + (uintptr_t)align - 1u) & ~((uintptr_t)align - 1u);

  // The asm does an extra sanity tst and errors if it fails; keep it.
  if ((aligned & ((uintptr_t)align - 1u)) != 0) {
    printk("kmalloc_aligned:%d: alignment computation failed\n", 0x66);
    clean_reboot();
    __builtin_unreachable();
  }

  // Advance heap pointer to the aligned boundary, then allocate normally.
  g_heap.ptr = aligned;
  return kmalloc(size);
}

// Resets allocator to the start of the heap.
void kfree_all(void) { g_heap.ptr = g_heap.start; }
