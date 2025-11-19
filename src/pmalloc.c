// tu: pubblico pm_* — smista small/large
// sk: in questo file decido se usare il buddy allocator o mmap in base alla dimensione richiesta
#include "pmalloc.h"
#include "buddy.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

// sk: allinea x verso l'alto al multiplo di a (es. 16 byte) per rispettare l'allineamento
static inline size_t align_up(size_t x, size_t a){
    return (x + (a - 1)) & ~(a - 1);
}
// sk: controllo se il puntatore appartiene a una allocazione "large" gestita con mmap
static int is_large_block(void* user_ptr){
    if (!user_ptr) return 0;
    // sk: torno all'indirizzo dell'header LargeHdr per leggere il magic e la size
    uint8_t* p = (uint8_t*)user_ptr - sizeof(LargeHdr);
    LargeHdr* h = (LargeHdr*)p;
    return h->magic == MAGIC_LARG;
}
// sk: implemntazione di malloc -> decide se andare su buddy o su mmap
void* pm_malloc(size_t size){
    if (size == 0) size = 1;
    size_t thr = pm_page_size() / 4;
// sk: soglia di separazione tra small e large = 1/4 della dimensione di pagina

    // sk: per richieste piccole uso il buddy allocator sulla arena da 1MiB 
    if (size < thr){
        return buddy_alloc_small(size);
    } else {
    // sk: per richieste grandi alloco una regione separata con mmap e ci metto un header LargeHdr
        size_t total = align_up(sizeof(LargeHdr) + size, ALIGNMENT);
        void* base = mmap(NULL, total, PROT_READ|PROT_WRITE,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return NULL;
    // sk: se mmap fallisce restituisco NULL come una malloc standard
        LargeHdr* h = (LargeHdr*)base;
        h->magic = MAGIC_LARG;
        h->size  = (uint32_t)total;
        return (void*)((uint8_t*)base + sizeof(LargeHdr));
    }
}
// sk: free -> capisce se il blocco ? small (buddy) o large (mmap) e lo libera correttamente
void pm_free(void* ptr){
    if (!ptr) return;
    // sk: blocco large ? recupero l'indirizzo di base e chiamo munmap sulla regione intera
    if (is_large_block(ptr)){
        uint8_t* base = (uint8_t*)ptr - sizeof(LargeHdr);
        LargeHdr* h = (LargeHdr*)base;
        size_t sz = h->size;
        h->magic = 0;
        munmap(base, sz);
        return;
    }
    // sk: blocco small ? delego al buddy allocator che fa coalescenza con i buddy liberi
    if (is_from_small_arena(ptr)){
        buddy_free_small(ptr);
        return;
    }
}
// sk: calloc -> alloca e azzera la memoria, con controllo de overflow su nmemb * size
void* pm_calloc(size_t nmemb, size_t size){
    // sk: prevenzione overflow: se nmemb*size non ci sta in size_t ritorno NULL
    if (nmemb && size && nmemb > (SIZE_MAX / size)) return NULL;
    size_t tot = nmemb * size;
    void* p = pm_malloc(tot);
    if (p) memset(p, 0, tot); //sk: imposto tutti i byte a zero come richiesto da calloc
    return p;
}
// sk: realloc -> gestisce i casi speciali e poi rialloca copiando il minimo tra vecchio e nuovo size 
void* pm_realloc(void* ptr, size_t size){

    // sk: rispetto il comportamento standard: realloc(NULL) = malloc, realloc(p,0) = free
    if (!ptr) return pm_malloc(size);
    if (size == 0){ pm_free(ptr); return NULL; }

    void* np = pm_malloc(size);
    if (!np) return NULL;

// sk: calcolo quanti byte copiare: non devo leggere oltre il vecchio payload
    size_t copy_sz = size;
// sk: blocco LARGE -> ricavo il payload precedente leggendo la size memorizzata nell'header
    if (is_large_block(ptr)){
        LargeHdr* h = (LargeHdr*)((uint8_t*)ptr - sizeof(LargeHdr));
        size_t old_payload = h->size - sizeof(LargeHdr);
        if (copy_sz > old_payload) copy_sz = old_payload;
// sk: blocco SMALL -> la dimensione precedente si calcola da 2^order meno l'header SmalHdr
    } else if (is_from_small_arena(ptr)){
        SmallHdr* h = (SmallHdr*)((uint8_t*)ptr - sizeof(SmallHdr));
        size_t old_payload = ((size_t)1 << h->order) - sizeof(SmallHdr);
        if (copy_sz > old_payload) copy_sz = old_payload;
    }
// sk: copio i dati dal vecchio blocco al nuovo e poi libero il vecchio
    memcpy(np, ptr, copy_sz);
    pm_free(ptr);
    return np;
}
