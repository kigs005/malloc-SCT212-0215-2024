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

static void coalesce(void)
{
    block_t *current = head;

    while (current != NULL && current->next != NULL) {
        block_t *next = current->next;

        uintptr_t current_end =
            (uintptr_t)current + HDR + current->size;

        if (current->free &&
            next->free &&
            current_end == (uintptr_t)next) {

            current->size += HDR + next->size;
            current->next = next->next;

            if (tail == next)
                tail = current;
        } else {
            current = current->next;
        }
    }
}

void my_free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_t *current = head;

    while (current != NULL) {
        if (payload(current) == ptr)
            break;

        current = current->next;
    }

    if (current == NULL) {
        static const char message[] = "my_free: invalid pointer\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    if (current->magic != MAGIC || current->free) {
        static const char message[] = "my_free: invalid or double free\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    current->free = 1;

    coalesce();
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > SIZE_MAX / nmemb)
        return NULL;

    size_t total_size = nmemb * size;

    void *ptr = my_malloc(total_size);

    if (ptr == NULL)
        return NULL;

    memset(ptr, 0, total_size);

    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return my_malloc(size);

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    size_t need = align_up(size);

    if (need == 0 || need > SIZE_MAX - HDR)
        return NULL;

    block_t *current = head;

    while (current != NULL) {
        if (payload(current) == ptr)
            break;

        current = current->next;
    }

    if (current == NULL || current->magic != MAGIC || current->free)
        return NULL;

    if (need <= current->size) {
        split_block(current, need);
        coalesce();
        return ptr;
    }

    while (current->next != NULL && current->next->free) {
        block_t *next = current->next;

        uintptr_t current_end =
            (uintptr_t)current + HDR + current->size;

        if (current_end != (uintptr_t)next)
            break;

        current->size += HDR + next->size;
        current->next = next->next;

        if (tail == next)
            tail = current;

        if (current->size >= need)
            break;
    }

    if (current->size >= need) {
        split_block(current, need);
        return ptr;
    }

    void *new_ptr = my_malloc(size);

    if (new_ptr == NULL)
        return NULL;

    size_t copy_size = current->size;

    if (copy_size > size)
        copy_size = size;

    memcpy(new_ptr, ptr, copy_size);

    my_free(ptr);

    return new_ptr;
}
