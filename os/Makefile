PROGS = kernel_entry.c
# Make sure user-progs is built before compiling PROGS
kernel_entry.c: user-progs

LIBS += ./vm/libvm.a
LIBS += ./fs/libfs.a

# COMMON_SRC += $(CS140E_2025_PATH)/libpi/libc/kmalloc.c

COMMON_SRC += os.c
COMMON_SRC += switchto-asm.S
COMMON_SRC += full-except-asm.S
COMMON_SRC += staff-full-except.c

STAFF_OBJS += ./staff-objs/staff-full-except-asm.o

BOOTLOADER=my-install
RUN = 1
CAN_EMIT=1

OUR_START = staff-start.S

EXCLUDE ?= grep -v simple_boot
# GREP_STR := 'TRACE:\|THREAD:\|ERROR:\|PANIC:'

GREP_STR := 'TRACE:\|SUCCESS:\|ERROR:\|PANIC:'

include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust-v2

./vm/libvm.a: FORCE
	make -C vm

./fs/libfs.a: FORCE
	make -C fs
	
user-progs: FORCE
	make -C user-progs

all:: ./vm/libvm.a ./fs/libfs.a user-progs $(PROGS:.c=.bin)

clean::
	make -C user-progs clean
	make -C vm clean
	make -C fs clean