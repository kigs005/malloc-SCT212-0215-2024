# Custom Memory Allocator — Performance Report

**Student Name:** Watiki John Kigotho 
**Student ID:** SCT212-0215/2024  

## 1. Introduction

This project involved designing and implementing a custom dynamic memory allocator in C using `sbrk()`. The allocator manages dynamically allocated memory through a linked list of memory blocks and supports allocation, deallocation, reallocation and heap diagnostics.

The main objective was to understand the internal operation of dynamic memory allocation, including memory alignment, free-block selection, splitting, coalescing, heap growth and fragmentation.

## 2. Implementation Overview

The allocator implements the following functions:

- `my_malloc()`: Allocates memory using the selected allocation policy.
- `my_free()`: Releases allocated memory for reuse.
- `my_calloc()`: Allocates memory and initializes it to zero.
- `my_realloc()`: Resizes an existing allocation while preserving its contents.
- `set_policy()`: Selects first-fit or best-fit allocation.
- `heap_stats()`: Reports heap usage and block statistics.
- `heap_dump()`: Displays block information for debugging.
- `heap_check()`: Checks the consistency of the allocator's internal block list.

The implementation uses 16-byte alignment and obtains memory through `sbrk()`. Heap growth requests at least 64 KiB, with larger requests when necessary. Free blocks can be split when the remaining space is sufficient to hold another block header and payload. Physically adjacent free blocks are merged through coalescing.

## 3. Testing and Validation

The supplied test program was executed using:

```bash
make clean
make
```

The test program contains eight test groups covering the allocator's required functionality.

### 3.1 Test Results

| Test | Description | Result |
|---|---|---|
| T1 | Basic allocation, alignment and `free(NULL)` | PASS |
| T2 | Allocated blocks do not overlap | PASS |
| T3 | Reuse of freed memory and block splitting | PASS |
| T4 | Coalescing adjacent free blocks | PASS |
| T5 | `calloc()`, `realloc()` and overflow handling | PASS |
| T6 | Heap growth through `sbrk()` | PASS |
| T7 | First-fit and best-fit allocation policies | PASS |
| T8 | Randomized stress test involving 20,000 operations | PASS |

**Final supplied test result: 29 passed, 0 failed.**

### 3.2 Detailed Observations

**T1 — Basic allocation**

Memory allocation returned a non-NULL pointer. The returned address satisfied the 16-byte alignment requirement. The heap consistency check passed, `malloc(0)` returned NULL, and freeing a NULL pointer caused no failure.

**T2 — Memory overlap**

Multiple allocated blocks were checked to ensure they did not overlap. Data written into the allocations remained intact, and the heap remained consistent after the blocks were freed.

**T3 — Reuse and splitting**

Previously freed memory was reused without unnecessary heap growth. The allocator split a sufficiently large free block, leaving a free remainder available for future allocations.

**T4 — Coalescing**

Freeing adjacent allocated blocks caused their physically contiguous free regions to merge. The resulting free block was sufficiently large to satisfy a subsequent 550-byte allocation request.

**T5 — `calloc()` and `realloc()`**

The calloc operation initialized allocated memory to zero. An integer-overflow request returned NULL. Reallocation preserved existing contents, shrinking preserved the original data prefix, `realloc(NULL, n)` behaved like allocation, and `realloc(p, 0)` freed the allocation and returned NULL.

**T6 — Heap growth**

A large allocation succeeded, and the heap grew to accommodate the request. The heap consistency check passed afterward.

**T7 — Allocation policies**

First-fit selected the first suitable free block, while best-fit selected the tightest suitable free block.

**T8 — Randomized stress testing**

The allocator completed 20,000 randomized allocation and deallocation operations without data corruption. After the allocated blocks were freed, the allocator reported that everything was free and the heap was fully coalesced to the extent expected by the test. The final heap consistency check passed.

### 3.3 Compiler and Sanitizer Validation

The test suite was also compiled and executed with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```

The sanitizer execution completed all eight test groups and reported:

**29 passed, 0 failed.**

No sanitizer errors were reported in the supplied output.

The compiler did report one warning about an unused `current_end` variable inside `heap_check()`. This warning was identified for correction before final submission. Therefore, a warning-free final build must be verified separately.

## 4. Benchmark Methodology

The benchmark compares three memory allocation approaches:

1. First-fit using the custom allocator.
2. Best-fit using the custom allocator.
3. The standard C library allocator using `malloc()` and `free()`.

Each benchmark run is designed to perform 50,000 deterministic allocation and deallocation operations. Requested allocation sizes range from 1 to 4096 bytes. A fixed random seed ensures that the same operation sequence is used for each allocator.

Execution time is measured using `clock_gettime()` with `CLOCK_MONOTONIC`.

For the custom allocator, the benchmark records heap bytes, used bytes, free bytes, total blocks, free blocks and the largest free block.

External fragmentation is calculated using:

\[
F = 1 - \frac{\text{largest free block}}{\text{total free bytes}}
\]

A lower fragmentation value indicates that a larger proportion of the free memory is concentrated in the largest available free block.

## 5. Benchmark Results

The benchmark measurements must be recorded after executing:

```bash
make benchmark
```

The confirmed allocator test results do not provide benchmark timing or fragmentation measurements. These are separate measurements and cannot be derived from the 29 supplied test assertions.

| Measurement | First-fit | Best-fit | libc |
|---|---:|---:|---:|
| Operations | 50,000 | 50,000 | 50,000 |
| Allocation size range | 1–4096 bytes | 1–4096 bytes | 1–4096 bytes |
| Random seed | 123456789 | 123456789 | 123456789 |
| Execution time | Pending measurement | Pending measurement | Pending measurement |
| Heap bytes | Pending measurement | Pending measurement | N/A |
| Used bytes after cleanup | Pending measurement | Pending measurement | N/A |
| Free bytes after cleanup | Pending measurement | Pending measurement | N/A |
| Total blocks | Pending measurement | Pending measurement | N/A |
| Free blocks | Pending measurement | Pending measurement | N/A |
| Largest free block | Pending measurement | Pending measurement | N/A |
| External fragmentation | Pending measurement | Pending measurement | N/A |

## 6. Results Analysis

The functional test results demonstrate that the allocator correctly performs the required allocation and memory management operations under the supplied test conditions. In particular, the successful reuse, splitting and coalescing tests demonstrate that the allocator can manage free memory rather than continuously requesting new memory from the operating system.

The randomized stress test provides additional evidence of correctness by exercising repeated allocation and deallocation operations while checking data integrity and heap consistency.

First-fit and best-fit have different search strategies. First-fit can terminate its search as soon as it finds a sufficiently large block, whereas best-fit searches for the smallest suitable block. Consequently, best-fit can require more search operations, although its effect on fragmentation depends on the allocation workload.

A quantitative performance comparison with libc requires the actual benchmark measurements. Execution time, heap statistics and external fragmentation should be compared before drawing conclusions about which strategy performs better. The supplied functional tests establish correctness, but do not independently establish that one allocator is faster or more memory-efficient than another.

## 7. Limitations

The implementation has several limitations:

- The singly linked list requires linear searches for allocation and pointer validation.
- The allocator does not provide general thread safety.
- Memory obtained using `sbrk()` is not generally returned to the operating system.
- The allocator is intended for educational purposes rather than production use.
- The benchmark represents a fixed workload and cannot predict performance under every real application workload.
- The implementation uses basic first-fit and best-fit placement policies rather than advanced allocation structures.

## 8. Conclusion

The project successfully implements a custom dynamic memory allocator using `sbrk()`. The supplied test program reported 29 passed tests and zero failures across eight test groups, including alignment, block reuse, splitting, coalescing, reallocation, heap growth, allocation policies and randomized stress testing.

The sanitizer execution also completed the supplied test suite without reporting sanitizer errors. The project demonstrates the fundamental principles of dynamic memory management, including allocation, deallocation, block organization and fragmentation.

The benchmark provides the basis for comparing first-fit, best-fit and the standard C library allocator. Final performance conclusions must be based on the measured benchmark results rather than assumptions about the allocation strategies.

## 9. References

1. Custom memory allocator assignment specification.
2. Course laboratory materials on dynamic memory allocation.
3. Linux manual page: `sbrk(2)` and `brk(2)`.
4. Linux manual page: `malloc(3)`.
5. Linux manual page: `clock_gettime(2)`.
6. C standard library documentation.

## 10. AI Assistance Declaration

AI assistance was used for technical explanations, debugging guidance, code review and documentation support during development. The implementation was tested against the supplied assignment test program. The student is responsible for understanding, reviewing and verifying the final submitted implementation and benchmark results.
