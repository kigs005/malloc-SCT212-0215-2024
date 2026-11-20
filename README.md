# Custom Memory Allocator

**Student Name:** Watiki John Kigotho  
**Student ID:** SCT212-0215/2024

## Project Overview

This project implements a custom dynamic memory allocator in C using `sbrk()`. It supports memory allocation, deallocation, reallocation, zero-initialized allocation, first-fit and best-fit placement policies, block splitting, coalescing, heap statistics and heap consistency checking.

## Project Files

| File | Description |
|---|---|
| `myalloc.c` | Custom allocator implementation |
| `myalloc.h` | Allocator interface supplied with the assignment |
| `test_myalloc.c` | Supplied allocator test program |
| `bench.c` | Performance benchmark |
| `Makefile` | Build, test and benchmark targets |
| `REPORT.md` | Benchmark results and analysis |
| `.gitignore` | Excludes generated binaries and temporary files |

## Build and Run

Build and execute the supplied tests:

```bash
make clean
make
```

Build the benchmark:

```bash
make bench
```

Execute the benchmark:

```bash
make benchmark
```

Remove generated executables and object files:

```bash
make clean
```

## Task Completion

| Task | Implementation | Status |
|---|---|---|
| Task 1 | Block metadata and alignment | [Verify] |
| Task 2 | Heap growth using `sbrk()` | [Verify] |
| Task 3 | First-fit allocation | [Verify] |
| Task 4 | Block splitting | [Verify] |
| Task 5 | Deallocation and coalescing | [Verify] |
| Task 6 | `calloc()` and `realloc()` | [Verify] |
| Task 7 | Best-fit allocation policy | [Verify] |
| Task 8 | Heap diagnostics and benchmarking | [Verify] |

## Design Explanation

The allocator manages memory using a singly linked list of block headers. Each header stores the payload size, a pointer to the next block, the allocation status and a metadata validation value. The list is maintained in address order so that blocks can be examined sequentially and physically adjacent free blocks can be identified.

All payloads are aligned to 16 bytes. The header size is rounded up to a multiple of 16 so that the payload following each header also starts at a properly aligned address. Memory is obtained from the process break using `sbrk()`. The allocator requests at least 64 KiB when growing the heap, or a larger amount when required by an allocation.

Allocation searches the free list using either first-fit or best-fit. First-fit returns the first sufficiently large free block. Best-fit searches for the smallest suitable block. A free block is split when the remainder is large enough to hold another header and aligned payload. During deallocation, the allocator merges neighboring blocks only when their memory addresses prove that they are physically contiguous.

The implementation also provides overflow checks, zero-initialized allocation, reallocation, heap statistics, a diagnostic heap dump and a consistency checker. The benchmark uses a fixed random seed and the same allocation/free workload for both custom policies and the system allocator.

## Questions and Answers

### 1. Why must the header size be a multiple of 16 as well as payload sizes?

The allocator must return payload addresses aligned to 16 bytes. If the header size were not a multiple of 16, adding it to an aligned header address could produce a misaligned payload address. Rounding the header size up to a multiple of 16 preserves alignment for every block.

### 2. Why can `printf()` inside `my_malloc()` cause infinite recursion?

Standard I/O functions may allocate internal buffers using the system memory allocator. If allocator diagnostics call `printf()` while `my_malloc()` is executing, that operation can trigger another allocation. This can cause recursive allocator calls and potentially infinite recursion. Low-level output such as `write()` avoids this particular dependency.

### 3. What extra field enables finding the previous neighbor in O(1), and what is the technique called?

A previous-block pointer in each block header allows the allocator to locate the previous block directly. This creates a doubly linked list. The technique is commonly associated with boundary-tag allocators when boundary metadata is also used to locate neighboring blocks.

### 4. What is the difference between internal and external fragmentation?

Internal fragmentation is wasted space inside an allocated block, often because the allocator rounds allocation sizes up for alignment or minimum block sizes. External fragmentation occurs when free memory is divided into separate non-contiguous blocks, so a large allocation may fail even when the total free memory is sufficient.

### 5. Why is memory obtained using `sbrk()` not normally returned to the OS, and how could memory at the heap end be returned?

The allocator cannot safely shrink the process break while allocated blocks remain above the proposed new break. Memory at the top of the heap can potentially be returned by lowering the program break with `brk()` or a negative `sbrk()` increment, provided the allocator can prove that the released region is free and is at the end of the heap.

### 6. How can consecutive list blocks fail to be physically adjacent, and what does coalescing do?

Other components of the process may change the program break between allocator growth operations, leaving gaps between blocks in the allocator's list. Coalescing merges free blocks only when their memory ranges are physically adjacent, creating larger free regions and reducing external fragmentation.

## Limitations

- The allocator uses a singly linked list, so allocation and deallocation searches can take O(n) time.
- Memory obtained from `sbrk()` is not generally returned to the operating system.
- The implementation is intended for a controlled educational environment, not as a production replacement for the system allocator.
- The benchmark represents one deterministic workload and does not predict performance for every application.
- The allocator is not designed for concurrent multi-threaded allocation.

## Sources

- Assignment specification supplied for this project.
- C standard library documentation.
- Linux manual pages: `sbrk(2)`, `brk(2)`, `clock_gettime(2)`, `malloc(3)`.
- Course materials on memory management and dynamic allocation.

## AI Assistance Declaration

AI assistance was used for explanations, debugging guidance, code review and documentation support during development. The implementation was tested and reviewed against the supplied assignment test program. The submitted work and its results should be understood and verified by the student.
