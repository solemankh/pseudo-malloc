#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* pm_malloc(size_t size);
void  pm_free(void* ptr);
void* pm_calloc(size_t nmemb, size_t size);
void* pm_realloc(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif
