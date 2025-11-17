# Pseudo Malloc (buddy + mmap)

Sostituto minimale di `malloc/free` in C per Linux:
- richieste **< (page_size / 4)**: gestite da **buddy allocator** su **arena 1 MiB**, con **bitmap** per i blocchi liberi/occupati;
- richieste **≥ (page_size / 4)**: gestite con `mmap()` dedicata.

✅ Conforme alle linee guida: valgrind, commit individuali, commenti con iniziali, no hardware.

## Build & Test

```bash
make
./build/test_alloc
valgrind --leak-check=full --show-leak-kinds=all ./build/test_alloc | tee docs/valgrind.log
```

### Prova come sostituto di `malloc` via LD_PRELOAD

```bash
make wrap
LD_PRELOAD=./build/libpmwrap.so ./build/test_ld_preload
```

### Stress script

```bash
bash scripts/stress.sh | tee docs/stress.out
```

## CI
- **GitHub Actions**: `.github/workflows/ci.yml`
- **GitLab CI**: `.gitlab-ci.yml`

## API
```c
void* pm_malloc(size_t size);
void  pm_free(void* ptr);
void* pm_calloc(size_t nmemb, size_t size);
void* pm_realloc(void* ptr, size_t size);
```

## Design
- Arena 1 MiB (mmap) a potenze di 2 da 16 B a 1 MiB.
- Bitmap per ordine (1 = libero, 0 = occupato).
- Split top-down, coalescenza bottom-up (buddy XOR 1).
- Large via `mmap` dedicata con header `LargeHdr`.
- Allineamento 16 B. Non thread-safe.

## Contributi e anti-plagio
- Metti le **tue iniziali** nei commenti (es. `//tu:`) dove modifichi.
- Fai commit frequenti e descrittivi.
