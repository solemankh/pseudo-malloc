// tu: pubblico pm_* — smista small/large
#include "pmalloc.h"
#include "buddy.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

static inline size_t align_up(size_t x, size_t a){
    return (x + (a - 1)) & ~(a - 1);
}
static int is_large_block(void* user_ptr){
    if (!user_ptr) return 0;
    uint8_t* p = (uint8_t*)user_ptr - sizeof(LargeHdr);
    LargeHdr* h = (LargeHdr*)p;
    return h->magic == MAGIC_LARG;
}

void* pm_malloc(size_t size){
    if (size == 0) size = 1;
    size_t thr = pm_page_size() / 4;

    if (size < thr){
        return buddy_alloc_small(size);
    } else {
        size_t total = align_up(sizeof(LargeHdr) + size, ALIGNMENT);
        void* base = mmap(NULL, total, PROT_READ|PROT_WRITE,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return NULL;
        LargeHdr* h = (LargeHdr*)base;
        h->magic = MAGIC_LARG;
        h->size  = (uint32_t)total;
        return (void*)((uint8_t*)base + sizeof(LargeHdr));
    }
}

void pm_free(void* ptr){
    if (!ptr) return;
    if (is_large_block(ptr)){
        uint8_t* base = (uint8_t*)ptr - sizeof(LargeHdr);
        LargeHdr* h = (LargeHdr*)base;
        size_t sz = h->size;
        h->magic = 0;
        munmap(base, sz);
        return;
    }
    if (is_from_small_arena(ptr)){
        buddy_free_small(ptr);
        return;
    }
}

void* pm_calloc(size_t nmemb, size_t size){
    if (nmemb && size && nmemb > (SIZE_MAX / size)) return NULL;
    size_t tot = nmemb * size;
    void* p = pm_malloc(tot);
    if (p) memset(p, 0, tot);
    return p;
}

void* pm_realloc(void* ptr, size_t size){
    if (!ptr) return pm_malloc(size);
    if (size == 0){ pm_free(ptr); return NULL; }

    void* np = pm_malloc(size);
    if (!np) return NULL;

    size_t copy_sz = size;
    if (is_large_block(ptr)){
        LargeHdr* h = (LargeHdr*)((uint8_t*)ptr - sizeof(LargeHdr));
        size_t old_payload = h->size - sizeof(LargeHdr);
        if (copy_sz > old_payload) copy_sz = old_payload;
    } else if (is_from_small_arena(ptr)){
        SmallHdr* h = (SmallHdr*)((uint8_t*)ptr - sizeof(SmallHdr));
        size_t old_payload = ((size_t)1 << h->order) - sizeof(SmallHdr);
        if (copy_sz > old_payload) copy_sz = old_payload;
    }
    memcpy(np, ptr, copy_sz);
    pm_free(ptr);
    return np;
}
