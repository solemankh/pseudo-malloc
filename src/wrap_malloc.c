#include "pmalloc.h"
#include <stddef.h>
void* malloc(size_t size){ return pm_malloc(size); }
void  free(void* ptr){ pm_free(ptr); }
void* calloc(size_t nmemb, size_t size){ return pm_calloc(nmemb, size); }
void* realloc(void* ptr, size_t size){ return pm_realloc(ptr, size); }
