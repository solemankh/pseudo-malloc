#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/pmalloc.h"

static int test_small(){
    printf("[small] ");
    void* p = pm_malloc(100);
    if (!p) return 1;
    memset(p, 0xAB, 100);
    pm_free(p);
    puts("ok");
    return 0;
}

static int test_large(){
    printf("[large] ");
    void* p = pm_malloc(1<<16);
    if (!p) return 1;
    memset(p, 0xCD, 1<<16);
    pm_free(p);
    puts("ok");
    return 0;
}

static int test_realloc(){
    printf("[realloc] ");
    char* p = (char*)pm_malloc(200);
    if (!p) return 1;
    strcpy(p, "hello");
    p = (char*)pm_realloc(p, 4000);
    if (!p) return 2;
    if (strcmp(p, "hello") != 0) return 3;
    p = (char*)pm_realloc(p, 8);
    if (!p) return 4;
    if (strcmp(p, "hello") != 0) return 5;
    pm_free(p);
    puts("ok");
    return 0;
}

static int test_calloc(){
    printf("[calloc] ");
    int n = 256;
    int* v = (int*)pm_calloc(n, sizeof(int));
    if (!v) return 1;
    for (int i=0;i<n;i++) if (v[i]!=0) return 2;
    pm_free(v);
    puts("ok");
    return 0;
}

int main(void){
    int rc = 0;
    rc |= test_small();
    rc |= test_large();
    rc |= test_realloc();
    rc |= test_calloc();
    puts(rc ? "TESTS FAIL" : "ALL TESTS PASSED");
    return rc;
}
