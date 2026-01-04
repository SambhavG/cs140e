# Complete Raspberry Pi OS

This is a complete Raspberry Pi OS implementation and debugger, based on the labs from [cs140e](https://github.com/dddrrreee/cs140e-25win).

It supports processes, concurrency, pinned virtual memory with variable page sizes, syscalls including forking, a FAT32 filesystem, preemptive threads via interrupts, and the ability to run programs from disk. 

The debugger operates hardware debugging registers to add single stepping on instructions, breakpointing, watchpointing, and general debugging operations.

It's complied with arm-none-eabi-gcc and can be installed with `./my-install kernel_entry.bin`, though this is handled by the makefile and `make all` in the os directory.

It is not fully standalone yet, but I hope to add the last few features to make it fully standalone soon.