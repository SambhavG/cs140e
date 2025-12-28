
PROGS = tests/5-user-prog.c

VM_DIR = $(CS140E_2025_PATH)/labs/os/code/vm
FS_DIR = $(CS140E_2025_PATH)/labs/os/code/fs

INC += -I$(VM_DIR)
INC += -I$(FS_DIR)
COMMON_SRC += $(FS_DIR)/fat32.c
COMMON_SRC += $(FS_DIR)/mbr.c
COMMON_SRC += $(FS_DIR)/pi-sd.c
COMMON_SRC += $(FS_DIR)/mbr-helpers.c
COMMON_SRC += $(FS_DIR)/fat32-helpers.c
COMMON_SRC += $(FS_DIR)/fat32-lfn-helpers.c
COMMON_SRC += $(FS_DIR)/external-code/unicode-utf8.c
COMMON_SRC += $(FS_DIR)/external-code/emmc.c
COMMON_SRC += $(CS140E_2025_PATH)/libpi/libc/kmalloc.c
COMMON_SRC += $(VM_DIR)/mmu.c  
COMMON_SRC += $(VM_DIR)/pinned-vm.h
COMMON_SRC += $(VM_DIR)/pinned-vm.c
COMMON_SRC += $(VM_DIR)/your-mmu-asm.S
COMMON_SRC += eqx-os.c
COMMON_SRC += switchto-asm.S
COMMON_SRC += full-except-asm.S
COMMON_SRC += staff-full-except.c

# STAFF_OBJS += $(PIN_DIR)/staff-pinned-vm.o  
# STAFF_OBJS += $(VM_DIR)/staff-mmu-except.o  
# STAFF_OBJS += ./staff-objs/staff-switchto-asm.o
# STAFF_OBJS += ./staff-objs/staff-breakpoint.o
STAFF_OBJS += ./staff-objs/staff-full-except-asm.o
# STAFF_OBJS += ./staff-objs/staff-mmu.o  
# STAFF_OBJS += $(VM_DIR)/staff-mmu-asm.o 


BOOTLOADER=my-install
RUN = 1
CAN_EMIT=1

OUR_START = staff-start.S

EXCLUDE ?= grep -v simple_boot
# GREP_STR := 'TRACE:\|THREAD:\|ERROR:\|PANIC:'

GREP_STR := 'TRACE:\|SUCCESS:\|ERROR:\|PANIC:'

include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust-v2

all:: user-progs $(PROGS:.c=.bin)

user-progs/byte-array-0-hello.h: FORCE
	make -C user-progs

user-progs: FORCE
	make -C user-progs

clean::
	make -C user-progs clean
