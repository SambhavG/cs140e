PROGS = my-install.c my-install-debug.c handler.c
COMMON_SRC = put-code.c
CFLAGS += -I../2-pi-side

include $(CS140E_2025_PATH)/libunix/mk/Makefile.unix

trace:
	./my-install --trace-control --last ./hello.bin

trace-all:
	./my-install --trace-all --last hello.bin

.PHONEY: trace trace-all
