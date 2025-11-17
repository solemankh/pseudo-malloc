# Report - Pseudo Malloc (Buddy + mmap)

## Obiettivo
Sostituto minimale di `malloc/free`:
- small (< page/4) → buddy + bitmap su 1 MiB
- large (>= page/4) → mmap

## Architettura
- `arena.c`: buddy con bitmap per ordine, split/coalescenza
- `pmalloc.c`: API, smistamento small/large, header
- `wrap_malloc.c`: LD_PRELOAD wrapper
- `tests/`: unit test
- `scripts/stress.sh`: stress

## Verifica
- `make && ./build/test_alloc`
- `valgrind --leak-check=full --show-leak-kinds=all ./build/test_alloc`
