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

_Static_assert(ALIGNMENT == 16, "Alignment must be 16 bytes");
_Static_assert((HDR % ALIGNMENT) == 0, "Header must be aligned");

static block_t *head = NULL;
static block_t *tail = NULL;

static size_t total_heap_bytes = 0;

static fit_policy_t current_policy = FIT_FIRST;

static size_t align_up(size_t size)
{
    if (size > SIZE_MAX - ALIGN_MASK)
        return 0;

    return (size + ALIGN_MASK) & ~(size_t)ALIGN_MASK;
}

static void *payload(block_t *block)
{
    return (char *)block + HDR;
}

static block_t *grow(size_t need)
{
    if (need > SIZE_MAX - HDR)
        return NULL;

    size_t total_size = need + HDR;

    if (total_size < CHUNK_SIZE)
        total_size = CHUNK_SIZE;

    void *current_break = sbrk(0);

    if (current_break == (void *)-1)
        return NULL;

    uintptr_t address = (uintptr_t)current_break;

    size_t padding =
        (ALIGNMENT - ((address + HDR) % ALIGNMENT)) % ALIGNMENT;

    if (total_size > SIZE_MAX - padding)
        return NULL;

    if (total_heap_bytes > SIZE_MAX - total_size - padding)
        return NULL;

    void *memory = sbrk(total_size + padding);

    if (memory == (void *)-1)
        return NULL;

    block_t *block = (block_t *)((char *)memory + padding);

    block->size = total_size - HDR;
    block->next = NULL;
    block->free = 1;
    block->magic = MAGIC;

    if (head == NULL)
        head = block;
    else
        tail->next = block;

    tail = block;

    total_heap_bytes += total_size + padding;

    return block;
}

static block_t *find_fit(size_t need)
{
    block_t *current = head;

    while (current != NULL) {
        if (current->free && current->size >= need)
            return current;

        current = current->next;
    }

    return NULL;
}

static void split_block(block_t *block, size_t need)
{
    if (block->size < need)
        return;

    if (block->size - need < HDR + ALIGNMENT)
        return;

    block_t *remainder =
        (block_t *)((char *)block + HDR + need);

    remainder->size = block->size - need - HDR;
    remainder->next = block->next;
    remainder->free = 1;
    remainder->magic = MAGIC;

    block->size = need;
    block->next = remainder;

    if (tail == block)
        tail = remainder;
}

void *my_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t need = align_up(size);

    if (need == 0)
        return NULL;

    if (need > SIZE_MAX - HDR)
        return NULL;

    block_t *block = find_fit(need);

    if (block == NULL)
        block = grow(need);

    if (block == NULL)
        return NULL;

    split_block(block, need);

    block->free = 0;

    return payload(block);
}
