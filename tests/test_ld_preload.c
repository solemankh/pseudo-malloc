#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    printf("LD_PRELOAD demo start\n");
    char* p = (char*)malloc(123);
    strcpy(p, "ciao preload");
    printf("str: %s\n", p);
    p = (char*)realloc(p, 1000);
    printf("len after realloc: %zu\n", strlen(p));
    free(p);
    puts("done");
    return 0;
}
