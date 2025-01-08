#include <stdio.h>
#include <stdlib.h>

// warm-up: what will the compiler do?
void foo(void) {
    int i = 0;
    return;
}

// what will the compiler do?
int bar(void) {
    int x = 3;
    int y = 4;
    int z = 5;
    int w = 6;

    return x*y+z*w;
}

#if 0
unsigned mul16(unsigned x) {
    return x * 16;
}


unsigned switchx(unsigned x) {
    int v = 0;

    switch(v) { 
    case 0:
        v += 1;
        v += 1;
        v += 1;
        v += 1;
        v += 1;

        break;
    case 1:
        printf("hello\n");
    default:
        exit(0);
    }
    return v;
}

void null(void) {
    *(char *)0 = 4;
    return;
}
#endif
