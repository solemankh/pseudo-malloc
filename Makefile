CC      := gcc
CFLAGS  := -O2 -g -std=c17 -Wall -Wextra -Wpedantic -Wconversion -fPIC
INCLUDE := -Iinclude
BUILD   := build

SRC_CORE := src/pmalloc.c src/arena.c src/buddy.c
WRAP_SRC := src/wrap_malloc.c

all: $(BUILD)/test_alloc $(BUILD)/test_ld_preload

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pmalloc.o: src/pmalloc.c include/pmalloc.h include/buddy.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/arena.o: src/arena.c include/buddy.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/buddy.o: src/buddy.c include/buddy.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/libpmalloc.a: $(BUILD)/pmalloc.o $(BUILD)/arena.o $(BUILD)/buddy.o
	ar rcs $@ $^

$(BUILD)/test_alloc: tests/test_alloc.c $(BUILD)/libpmalloc.a | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@

$(BUILD)/test_ld_preload: tests/test_ld_preload.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@

wrap: $(BUILD)/libpmwrap.so

$(BUILD)/wrap_malloc.o: src/wrap_malloc.c include/pmalloc.h | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/libpmwrap.so: $(BUILD)/wrap_malloc.o $(BUILD)/libpmalloc.a
	$(CC) -shared -o $@ $(BUILD)/wrap_malloc.o $(BUILD)/pmalloc.o $(BUILD)/arena.o $(BUILD)/buddy.o

clean:
	rm -rf $(BUILD)

.PHONY: all clean wrap
