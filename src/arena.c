// tu: buddy allocator con BITMAP su 1 MiB
// sk: includo l'header del buddy allocator dove sono definite le costanti e le strutture

// sk: includo librerie standard per mmap, memoria, e funzioni di utilit?
#include "buddy.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// sk: questo file implementa il buddy allocator usato per piccole allocazioni (< 1/4 pagina)
// sk: tutta l'area di memoria gestita ? una singola arena da 1MiB allocata con mmap

// sk: numero di livelli (ordini) del buddy, da MIN_ORDER a MAX_ORDER

#define NUM_ORDERS ((MAX_ORDER)-(MIN_ORDER)+1)


// sk: puntatore alla base dell'arena di memoria dove il buddy alloca i blocchi small
static uint8_t* arena_base = NULL;

// sk: struttura che rappresenta una bitmap (array di 64 bit) usata per segnare i blocchi liberi/occupati
typedef struct {
    uint64_t* words;
    size_t    n_bits;
    size_t    n_words;
} Bitset;

// sk: array di bitmap, una per ogni ordine del buddy (da MIN_ORDER a MAX_ORDER)
static Bitset free_bm[NUM_ORDERS];

// sk: funzione di utilit? per ottenere la dimensione della pagina del sistema (di solito 4096B)
size_t pm_page_size(void) {
    static size_t ps = 0;
    if (!ps) {
// sk: uso sysconf per leggere la dimensione reale della pagina dal sistema operativo
        long p = sysconf(_SC_PAGESIZE);
        ps = (p > 0) ? (size_t)p : 4096u;
    }
    return ps;
}

// sk: allinea un valore verso l'alto al multiplo di 'a' (per esempio 16 byte)
static inline size_t align_up(size_t x, size_t a){ return (x + (a-1)) & ~(a-1); }

// sk: arrotonda x alla potenza di 2 pi? vicina (necessario per trovare l'ordine del buddy)
static inline size_t round_up_pow2(size_t x){
    if (x <= 1) return 1;
    x--; x|=x>>1; x|=x>>2; x|=x>>4; x|=x>>8; x|=x>>16;
#if UINTPTR_MAX > 0xffffffffu
    x|=x>>32;
#endif
    return x+1;
}

// sk: converto un ordine (es. 4..20) in indice dell'array free_bm (partendo da 0)
static inline int ord_idx(int order){ return order - MIN_ORDER; }

// sk: calcolo quanti blocchi di quell'ordine ci stanno dentro l'arena da 1MiB
static inline size_t blocks_at_order(int order){ return (size_t)1 << (MAX_ORDER - order); }

// sk: inizializzo la bitmap allocando abbastanza word da 64 bit per contenere n_bits
static void bitset_init(Bitset* b, size_t n_bits){
    b->n_bits = n_bits;
    b->n_words = (n_bits + 63) / 64;
    b->words = (uint64_t*)calloc(b->n_words, sizeof(uint64_t));
    if (!b->words){ perror("calloc bitmap"); abort(); }
}

// sk: imposta a 1 il bit i-esimo (segna il blocco come LIBERO)
static inline void bit_set(Bitset* b, size_t i){
    if (i >= b->n_bits) return;
    b->words[i>>6] |= (uint64_t)1ULL << (i & 63);
}

// sk: mette a 0 il bit i-esimo (blocca occupato dal buddy)
static inline void bit_clr(Bitset* b, size_t i){
    if (i >= b->n_bits) return;
    b->words[i>>6] &= ~((uint64_t)1ULL << (i & 63));
}


// sk: legge il valore del bit i-esimo per sapere se il blocco ? libero
static inline bool bit_get(Bitset* b, size_t i){
    if (i >= b->n_bits) return false;
    return (b->words[i>>6] >> (i & 63)) & 1ULL;
}
static size_t bit_find_first(Bitset* b){
    // sk: scorro le word della bitmap e cerco la prima che contiene almeno un bit a 1
    for (size_t w=0; w<b->n_words; ++w){
        uint64_t x = b->words[w];
        if (!x) continue;
        // sk: uso ctz (count trailing zeros) per trovare l'indice del primo bit a 1
        unsigned tz = (unsigned)__builtin_ctzll(x);
        return (w<<6) + tz;
    }
    return (size_t)-1;
}

// sk: converto un indirizzo della arena nell'indice del blocco al dato ordine
static inline size_t addr_to_index(void* p, int order){
    uintptr_t off = (uintptr_t)((uint8_t*)p - arena_base);
    return (size_t)(off >> order);
}
// sk: operazione inversa: dall'indice del blocco ricavo l'indirizzo nella arena
static inline void* index_to_addr(size_t idx, int order){
    return (void*)(arena_base + ((size_t)idx << order));
}

void buddy_init(void){
// sk: inizializzo l'arena solo la prima volta che viene chiamata
    if (arena_base) return;
// sk: alloca 1 MiB di memoria anonima che user? come heap del buddy allocator
    void* base = mmap(NULL, ARENA_SIZE, PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED){ perror("mmap arena"); abort(); }
    arena_base = (uint8_t*)base;
// sk: per ogni ordine preparo una bitmap che tiene traccia dei blocchi liberi
    for (int o=MIN_ORDER; o<=MAX_ORDER; ++o){
        bitset_init(&free_bm[ord_idx(o)], blocks_at_order(o));
    }
    bit_set(&free_bm[ord_idx(MAX_ORDER)], 0); // 1 blocco libero da 1 MiB
}

// sk: cerca un blocco libero a partire dall'ordine richiesto e, se serve, fa gli split verso ordini pi? piccoli
static void* alloc_order_bitmap(int want_order){
// sk: provo tutti gli ordini da quello richiesto fino al massimo per trovare un blocco libero
    for (int o = want_order; o <= MAX_ORDER; ++o){
        Bitset* B = &free_bm[ord_idx(o)];
        size_t idx = bit_find_first(B);
        if (idx == (size_t)-1) continue;
        bit_clr(B, idx);
        while (o > want_order){
            // sk: divido ricorsivamente il blocco in due buddy finch? arrivo all'ordine richiesto
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

// sk: alloca un blocco "small" dentro l'arena usando il buddy allocator
void* buddy_alloc_small(size_t payload_size){
// sk: inizializzo l'arena la prima volta che viene chiamata l'allocazione 
    if (!arena_base) buddy_init();
// sk: considero anche l'header SmallHdr e allineo la dimensione richiesta
    size_t need = align_up(sizeof(SmallHdr) + payload_size, ALIGNMENT);
// sk: porto la dimensione alla potenza di 2 successiva per scegliere l'ordine corretto
    size_t blk_size = round_up_pow2(need < (1u<<MIN_ORDER) ? (1u<<MIN_ORDER) : need);
    int order = MIN_ORDER;
// sk: aumento l'ordine finch? il blocco di quell'ordine ? abbastanza grande
    while (((size_t)1<<order) < blk_size) order++;
    if (order > MAX_ORDER) return NULL;

// sk: cerco un blocco libero a quell'ordine (con eventuali split)
    void* blk = alloc_order_bitmap(order);
    if (!blk) return NULL;

// sk: scrivo l'header all'inizio del blocco e restituisco il puntatore all'area utente
    SmallHdr* h = (SmallHdr*)blk;
    h->magic = MAGIC_SMAL;
    h->order = (uint16_t)order;
    return (void*)((uint8_t*)blk + sizeof(SmallHdr));
}

// sk: libera un blocco e prova a fonderlo con il buddy risalendo di ordine 
static void coalesce_and_free_bitmap(void* blk, int order){
// sk: converto l'indirizzo del blocco nell'ordine della bitmap
    size_t idx = addr_to_index(blk, order);
    bit_set(&free_bm[ord_idx(order)], idx);
    while (order < MAX_ORDER){
// sk: calcolo l'indice del buddy (xor con 1)
        size_t bud_idx = idx ^ 1ULL;
        Bitset* B = &free_bm[ord_idx(order)];
        if (bit_get(B, bud_idx)){
// sk: se anche il buddy ? libero, unisco i due blocchi e salgo di ordine
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

// sk: free per blocchi small: recupero e delega a coalesce_and_free_bitmap
void buddy_free_small(void* user_ptr){
    if (!user_ptr) return;
// sk: dal puntatore utente torno indietro di sizeof(SmallHdr) per leggere l'header
    uint8_t* raw = (uint8_t*)user_ptr - sizeof(SmallHdr);
    SmallHdr* h = (SmallHdr*)raw;
// sk: se il magic non ? quello atteso, non tocco la memoria (possibile errore dell'utente)
    if (h->magic != MAGIC_SMAL) return;
    int order = h->order;
    h->magic = 0; h->order = 0;
    coalesce_and_free_bitmap((void*)raw, order);
}

// sk: controllo se un puntatore appartiene all'arena small gestita dal buddy
bool is_from_small_arena(void* user_ptr){
    if (!arena_base || !user_ptr) return false;
    uint8_t* p = (uint8_t*)user_ptr;
// sk: verifico che il puntatore cada dentro [arena_base + header, arena_base + ARENA_SIZE)
    if (p < arena_base + sizeof(SmallHdr) || p >= arena_base + ARENA_SIZE) return false;
    SmallHdr* h = (SmallHdr*)(p - sizeof(SmallHdr));
    return h->magic == MAGIC_SMAL;
}
