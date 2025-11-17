#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ARENA_SIZE      (1u<<20)   // 1 MiB
#define MIN_ORDER       4          // 16 B
#define MAX_ORDER       20         // 1 MiB
#define ALIGNMENT       16

typedef struct SmallHdr {
    uint32_t magic;   // 'SMAL'
    uint16_t order;   // ordine buddy
    uint16_t pad;
} SmallHdr;

typedef struct LargeHdr {
    uint32_t magic;   // 'LARG'
    uint32_t size;    // bytes mappati (incl. header)
} LargeHdr;

#define MAGIC_SMAL 0x534D414Lu
#define MAGIC_LARG 0x4C415247u

void   buddy_init(void);
void*  buddy_alloc_small(size_t payload_size);
void   buddy_free_small(void* user_ptr);
bool   is_from_small_arena(void* user_ptr);
size_t pm_page_size(void);
