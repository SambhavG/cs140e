// convert the contents of stdin to their ASCII values (e.g.,
// '\n' = 10) and spit out the <prog> array used in Figure 1 in
// Thompson's paper.
#include <stdio.h>

int main(void) {
    char input[10000];

    int i = 0;
    char result;
    do {
        result = getchar();
        input[i] = result;
        i++;
    } while (result != -1);
    input[i - 1] = 0;

    printf("char prog[] = {\n");
    for (int j = 0; j < i - 1; j++) {
        printf("\t%d,%c", input[j], (j + 1) % 8 == 0 ? '\n' : ' ');
    }
    printf("0 };\n");
    printf("%s", input);

    return 0;
}
