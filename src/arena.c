// tu: buddy allocator con BITMAP su 1 MiB
#include "buddy.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define NUM_ORDERS ((MAX_ORDER)-(MIN_ORDER)+1)

static uint8_t* arena_base = NULL;

typedef struct {
    uint64_t* words;
    size_t    n_bits;
    size_t    n_words;
} Bitset;

static Bitset free_bm[NUM_ORDERS];

size_t pm_page_size(void) {
    static size_t ps = 0;
    if (!ps) {
        long p = sysconf(_SC_PAGESIZE);
        ps = (p > 0) ? (size_t)p : 4096u;
    }
    return ps;
}
static inline size_t align_up(size_t x, size_t a){ return (x + (a-1)) & ~(a-1); }
static inline size_t round_up_pow2(size_t x){
    if (x <= 1) return 1;
    x--; x|=x>>1; x|=x>>2; x|=x>>4; x|=x>>8; x|=x>>16;
#if UINTPTR_MAX > 0xffffffffu
    x|=x>>32;
#endif
    return x+1;
}
static inline int ord_idx(int order){ return order - MIN_ORDER; }
static inline size_t blocks_at_order(int order){ return (size_t)1 << (MAX_ORDER - order); }

static void bitset_init(Bitset* b, size_t n_bits){
    b->n_bits = n_bits;
    b->n_words = (n_bits + 63) / 64;
    b->words = (uint64_t*)calloc(b->n_words, sizeof(uint64_t));
    if (!b->words){ perror("calloc bitmap"); abort(); }
}
static inline void bit_set(Bitset* b, size_t i){
    if (i >= b->n_bits) return;
    b->words[i>>6] |= (uint64_t)1ULL << (i & 63);
}
static inline void bit_clr(Bitset* b, size_t i){
    if (i >= b->n_bits) return;
    b->words[i>>6] &= ~((uint64_t)1ULL << (i & 63));
}
static inline bool bit_get(Bitset* b, size_t i){
    if (i >= b->n_bits) return false;
    return (b->words[i>>6] >> (i & 63)) & 1ULL;
}
static size_t bit_find_first(Bitset* b){
    for (size_t w=0; w<b->n_words; ++w){
        uint64_t x = b->words[w];
        if (!x) continue;
        unsigned tz = (unsigned)__builtin_ctzll(x);
        return (w<<6) + tz;
    }
    return (size_t)-1;
}

static inline size_t addr_to_index(void* p, int order){
    uintptr_t off = (uintptr_t)((uint8_t*)p - arena_base);
    return (size_t)(off >> order);
}
static inline void* index_to_addr(size_t idx, int order){
    return (void*)(arena_base + ((size_t)idx << order));
}

void buddy_init(void){
    if (arena_base) return;
    void* base = mmap(NULL, ARENA_SIZE, PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED){ perror("mmap arena"); abort(); }
    arena_base = (uint8_t*)base;
    for (int o=MIN_ORDER; o<=MAX_ORDER; ++o){
        bitset_init(&free_bm[ord_idx(o)], blocks_at_order(o));
    }
    bit_set(&free_bm[ord_idx(MAX_ORDER)], 0); // 1 blocco libero da 1 MiB
}

static void* alloc_order_bitmap(int want_order){
    for (int o = want_order; o <= MAX_ORDER; ++o){
        Bitset* B = &free_bm[ord_idx(o)];
        size_t idx = bit_find_first(B);
        if (idx == (size_t)-1) continue;
        bit_clr(B, idx);
        while (o > want_order){
            o--;
            size_t left_idx = idx * 2;
            size_t right_idx = left_idx + 1;
            Bitset* Bo = &free_bm[ord_idx(o)];
            bit_set(Bo, left_idx);
            bit_set(Bo, right_idx);
            bit_clr(Bo, left_idx);
            idx = left_idx;
        }
        return index_to_addr(idx, want_order);
    }
    return NULL;
}

void* buddy_alloc_small(size_t payload_size){
    if (!arena_base) buddy_init();
    size_t need = align_up(sizeof(SmallHdr) + payload_size, ALIGNMENT);
    size_t blk_size = round_up_pow2(need < (1u<<MIN_ORDER) ? (1u<<MIN_ORDER) : need);
    int order = MIN_ORDER;
    while (((size_t)1<<order) < blk_size) order++;
    if (order > MAX_ORDER) return NULL;

    void* blk = alloc_order_bitmap(order);
    if (!blk) return NULL;

    SmallHdr* h = (SmallHdr*)blk;
    h->magic = MAGIC_SMAL;
    h->order = (uint16_t)order;
    return (void*)((uint8_t*)blk + sizeof(SmallHdr));
}

static void coalesce_and_free_bitmap(void* blk, int order){
    size_t idx = addr_to_index(blk, order);
    bit_set(&free_bm[ord_idx(order)], idx);
    while (order < MAX_ORDER){
        size_t bud_idx = idx ^ 1ULL;
        Bitset* B = &free_bm[ord_idx(order)];
        if (bit_get(B, bud_idx)){
            bit_clr(B, bud_idx);
            bit_clr(B, idx);
            size_t parent_idx = idx >> 1;
            Bitset* P = &free_bm[ord_idx(order+1)];
            bit_set(P, parent_idx);
            idx = parent_idx;
            order++;
        } else break;
    }
}

void buddy_free_small(void* user_ptr){
    if (!user_ptr) return;
    uint8_t* raw = (uint8_t*)user_ptr - sizeof(SmallHdr);
    SmallHdr* h = (SmallHdr*)raw;
    if (h->magic != MAGIC_SMAL) return;
    int order = h->order;
    h->magic = 0; h->order = 0;
    coalesce_and_free_bitmap((void*)raw, order);
}

bool is_from_small_arena(void* user_ptr){
    if (!arena_base || !user_ptr) return false;
    uint8_t* p = (uint8_t*)user_ptr;
    if (p < arena_base + sizeof(SmallHdr) || p >= arena_base + ARENA_SIZE) return false;
    SmallHdr* h = (SmallHdr*)(p - sizeof(SmallHdr));
    return h->magic == MAGIC_SMAL;
}
