LIBNAME = libvm.a

BOOTLOADER = my-install

COMMON_SRC += pinned-vm.c
COMMON_SRC += mmu-helpers.c
COMMON_SRC += your-mmu-asm.S
COMMON_SRC += mmu.c
COMMON_SRC += mmu.h
COMMON_SRC += pt-vm.c


# YOUR_VM = $(CS140E_2025_PATH)/labs/15-vm-coherence/code.staff/

O = $(CS140E_2025_PATH)/libpi
# STAFF_OBJS += $(O)/staff-objs/staff-full-except.o
# STAFF_OBJS += $(O)/staff-objs/staff-full-except-asm.o
# STAFF_OBJS += $(O)/staff-objs/staff-switchto-asm.o
# STAFF_OBJS += $(O)/staff-objs/kmalloc.o

TTYUSB = 

# set RUN = 1 if you want the code to automatically run after building.
RUN = 1

GREP_STR := 'TRACE:\|HASH:\|ERROR:\|PANIC:\|SUCCESS:'
include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust-v2

clean::
	rm -f *.o *.a

