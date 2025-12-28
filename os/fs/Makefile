LIBNAME = libfs.a

CFLAGS_EXTRA  = -Iexternal-code

COMMON_SRC += $(CS140E_2025_PATH)/libpi/libc/kmalloc.c
COMMON_SRC += pi-sd.c mbr.c mbr-helpers.c fat32.c fat32-helpers.c fat32-lfn-helpers.c external-code/unicode-utf8.c external-code/emmc.c

TTYUSB = 
BOOTLOADER = my-install
RUN = 1

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|PASS:\|TEST:'
include $(CS140E_2025_PATH)/libpi/mk/Makefile.robust

clean::
	make -C external-code clean
	rm -f *.o *.a