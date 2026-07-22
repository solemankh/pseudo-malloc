# Pseudo Malloc with Buddy Allocator

## Overview

Pseudo Malloc is an educational memory allocator implemented in C for the Operating Systems course at Sapienza University of Rome.

The project provides a custom implementation of dynamic memory allocation based on a buddy allocator using a bitmap tree representation. Memory is obtained from the operating system through `mmap()` and managed manually without relying on the standard C library allocator.

The allocator supports memory allocation, deallocation, block splitting, buddy merging and consistency verification through dedicated test cases.

---

## Key Features

- Custom implementation of `malloc` and `free`
- Buddy Allocator based on bitmap tree representation
- Memory management using POSIX `mmap`
- Automatic block splitting during allocation
- Buddy block merging during deallocation
- Memory consistency verification
- Modular implementation with dedicated test suite

---

## Implemented API

```c
void* pseudo_malloc(size_t size);
void pseudo_free(void* ptr);
```

The allocator exposes a simple interface similar to the standard C memory allocation functions while internally managing memory through the buddy allocation algorithm.

---

## Requirements

- Linux or another POSIX-compatible environment
- GCC
- Make

The project can also be compiled and executed using Windows Subsystem for Linux (WSL).

---

## Compilation

```bash
make
```

---

## Running Tests

```bash
./test_pmalloc
```

---

## Technologies

- C
- POSIX API
- mmap
- Bitmap Tree Buddy Allocator
- Makefile
- Linux / WSL

---

## Learning Objectives

This project was developed to gain practical experience with:

- Dynamic memory allocation
- Buddy allocation algorithms
- Bitmap tree data structures
- Operating Systems memory management
- POSIX system calls
- Low-level programming in C

---

## Project Context

Academic project developed for the Operating Systems course within the Bachelor's Degree in Computer Engineering and Automation at Sapienza University of Rome.
