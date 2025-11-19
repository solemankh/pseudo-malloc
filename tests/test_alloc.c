// sk: test personali per verificare il corretto funzionamento di pm_malloc/pm_free/pm_realloc/pm_calloc

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/pmalloc.h"

// sk: verifica una piccola allocazione (dovrebbe finire nel buddy allocator)
static int test_small(){
    printf("[small] ");
    void* p = pm_malloc(100);
    if (!p) return 1;
// sk: riempio la memoria con un pattern per controllare che sia realmente utilizzabile 
    memset(p, 0xAB, 100);
    pm_free(p);
    puts("ok");
    return 0;
}

// sk: verifica una allocazione grande (64 KiB) che dovrebbe usare mmap
static int test_large(){
    printf("[large] ");
    void* p = pm_malloc(1<<16);
    if (!p) return 1;
// sk: scrivo un pattern diverso per essere sicuro che la memoria sia accessibile 
    memset(p, 0xCD, 1<<16);
    pm_free(p);
    puts("ok");
    return 0;
}

// sk: test pm_realloc: controllo che il contenuto venga preservato nei cambi di dimensione 
static int test_realloc(){
    printf("[realloc] ");
    char* p = (char*)pm_malloc(200);
    if (!p) return 1;
// sk: salva una stringa conosciuta che poi devo ritrovare dopo le realloc
    strcpy(p, "hello");
// sk: aumento molto la dimensione per forzare uno spostamento del blocco
    p = (char*)pm_realloc(p, 4000);
    if (!p) return 2;
    if (strcmp(p, "hello") != 0) return 3;
// sk: poi riduco la dimensione per verificare che realloc gestisca anche shrink
    p = (char*)pm_realloc(p, 8);
    if (!p) return 4;
    if (strcmp(p, "hello") != 0) return 5;
    pm_free(p);
    puts("ok");
    return 0;
}

// sk: test per pm_calloc: la memoria deve essere azzerata (tutti gli int a 0)
static int test_calloc(){
    printf("[calloc] ");
    int n = 256;
    int* v = (int*)pm_calloc(n, sizeof(int));
    if (!v) return 1;
// sk: se trovo un elemento diverso da zero significa che calloc non ha azzerato tutto
    for (int i=0;i<n;i++) if (v[i]!=0) return 2;
    pm_free(v);
    puts("ok");
    return 0;
}

int main(void){
// sk: eseguo in sequenza tutti i test e riporto un unico codice di ritorno
    int rc = 0;
    rc |= test_small();
    rc |= test_large();
    rc |= test_realloc();
    rc |= test_calloc();
    puts(rc ? "TESTS FAIL" : "ALL TESTS PASSED");
    return rc;
}
