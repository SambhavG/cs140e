Right now, we can start a program which can fork and exec, and the threads we create will be swapped between until all run

What we want in the end:
- A terminal on a screen which displays a terminal program, which will be one process in the OS. Initially it's the only process and it goes infinitely. 
- We want the following commands: cd, ls, cat, echo, and to run a program. Ideally aforementioned programs are programs on the sd
- We need a way to give args to a program that we want to run
- We need a way to give 