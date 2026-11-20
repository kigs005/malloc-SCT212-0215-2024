#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "myalloc.h"


#define ALIGNMENT 16
#define ALIGN_MASK (ALIGNMENT - 1)

#define MAGIC UINT32_C(0xCAFEBABE)

#define CHUNK_SIZE (64 * 1024)


typedef struct block {
    size_t size;
    struct block *next;
    int free;
    uint32_t magic;
} block_t;


#define HDR \
    ((sizeof(block_t) + ALIGN_MASK) & ~(size_t)ALIGN_MASK)


_Static_assert(
    ALIGNMENT == 16,
    "Allocator alignment must be 16 bytes"
);

_Static_assert(
    (HDR % ALIGNMENT) == 0,
    "Block header size must be a multiple of 16"
);



static block_t *head = NULL;
static block_t *tail = NULL;

static size_t total_heap_bytes = 0;

static fit_policy_t current_policy = FIT_FIRST;
