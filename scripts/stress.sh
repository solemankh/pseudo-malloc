#!/usr/bin/env bash
set -euo pipefail

echo "[stress] start"
g++ -x c - <<'EOF' -O2 -o /tmp/stress_bin
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(){
    srand(123);
    const int N=50000;
    void* ptrs[1000]={0};
    for(int i=0;i<N;i++){
        int idx = rand()%1000;
        if (ptrs[idx]){
            if (rand()%3==0){
                size_t n = (rand()%8192)+1;
                void* q = realloc(ptrs[idx], n);
                if(q) ptrs[idx]=q;
            } else {
                free(ptrs[idx]);
                ptrs[idx]=NULL;
            }
        } else {
            size_t n = (rand()%8192)+1;
            ptrs[idx]=malloc(n);
            if(ptrs[idx]) memset(ptrs[idx], 0xA5, n);
        }
    }
    for(int i=0;i<1000;i++) if (ptrs[i]) free(ptrs[i]);
    puts("stress ok");
    return 0;
}
EOF

make wrap >/dev/null
LD_PRELOAD=./build/libpmwrap.so /tmp/stress_bin
echo "[stress] end"
