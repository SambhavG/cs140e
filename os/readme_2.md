# Complete OS


## Desired flow
- there will be a single init.c file that, when bootloaded, will be the init process.
- It will register all the interrupts, init the virtual memory, init and read the fat32 filesystem. Then it will spawn a shell process that communicates with the host computer via UART.
- After the bootloading, the init process will turn into a process manager. It will be one while loop, which will repeatedly start a timer for 4 ms, jump into a child process, and jump back to itself whenever the timer expires, a child process returns, or a uart interrupt happens. There will be space for infinite processes (just assume for now that we'll never make enough to exceed memory limits), but one process will always be the shell, alongside the main init.c process that orchestrates.
- Each process including shell will get some virtual memory, probably 1/64 of available (so vmem will be configured with 8mb per process)
- Each "process" is a "thread" - there is no "multithreading", only "multiprocessing" 
- The shell will have a directory it's at, starting with /. It can cat, ls, cd, and write and save files to disk. It can eventually compile and run c programs from disk, making it self-contained.