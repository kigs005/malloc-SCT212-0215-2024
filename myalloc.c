#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "myalloc.h"

#define MAGIC UINT32_C(0xCAFEBABE)
#define CHUNK_SIZE (64 * 1024)

/* Block metadata structure */
typedef struct block {
    size_t size;
    struct block *next;
    int free;
    uint32_t magic;
} block_t;

/* Ensure header size is a multiple of 16 */
#define HDR ((sizeof(block_t) + 15) & ~(size_t)15)

/* Global allocator state */
static block_t *head = NULL;
static block_t *tail = NULL;

static size_t total_heap_bytes = 0;

static fit_policy_t current_policy = FIT_FIRST;

/* Align requested sizes to 16 bytes */
static size_t align_up(size_t n)
{
    return (n + 15) & ~(size_t)15;
}

/* Get the payload address */
static void *payload(block_t *block)
{
    return (char *)block + HDR;
}

/* Get the header from a payload pointer */
static block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

/* Request more memory from the operating system */
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
        (16 - ((address + HDR) % 16)) % 16;

    if (total_size > SIZE_MAX - padding)
        return NULL;

    void *memory = sbrk(total_size + padding);

    if (memory == (void *)-1)
        return NULL;

    block_t *block =
        (block_t *)((char *)memory + padding);

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

/* Find a suitable block using the selected allocation policy */
static block_t *find_fit(size_t need)
{
    block_t *current = head;
    block_t *best = NULL;

    /* First-fit: return the first suitable block */
    if (current_policy == FIT_FIRST) {

        while (current != NULL) {

            if (current->free &&
                current->size >= need &&
                current->magic == MAGIC) {

                return current;
            }

            current = current->next;
        }

        return NULL;
    }

    /* Best-fit: find the smallest suitable block */
    if (current_policy == FIT_BEST) {

        while (current != NULL) {

            if (current->free &&
                current->size >= need &&
                current->magic == MAGIC) {

                if (best == NULL ||
                    current->size < best->size) {

                    best = current;
                }
            }

            current = current->next;
        }

        return best;
    }

    return NULL;
}

/* Split a large free block when enough space remains */
static void split_block(block_t *block, size_t need)
{
    if (block->size < need ||
        block->size - need < HDR + 16)
        return;

    block_t *remainder = (block_t *)(
        (char *)block + HDR + need
    );

    remainder->size =
        block->size - need - HDR;

    remainder->next = block->next;
    remainder->free = 1;
    remainder->magic = MAGIC;

    block->size = need;
    block->next = remainder;

    if (tail == block)
        tail = remainder;
}

/* Allocate memory */
void *my_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    if (size > SIZE_MAX - 15)
        return NULL;

    size_t need = align_up(size);

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

/* Merge physically adjacent free blocks */
static void coalesce(void)
{
    block_t *current = head;

    while (current != NULL && current->next != NULL) {

        block_t *next = current->next;

        void *current_end =
            (char *)current + HDR + current->size;

        if (current->free &&
            next->free &&
            current->magic == MAGIC &&
            next->magic == MAGIC &&
            current_end == (void *)next) {

            current->size += HDR + next->size;
            current->next = next->next;

            if (tail == next)
                tail = current;
        }
        else {
            current = current->next;
        }
    }
}

/* Free allocated memory */
void my_free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_t *current = head;

    /* Validate pointer by searching our block list */
    while (current != NULL) {

        if (payload(current) == ptr)
            break;

        current = current->next;
    }

    if (current == NULL) {
        const char message[] =
            "my_free: invalid pointer\n";

        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    block_t *block = header(ptr);

    if (block->magic != MAGIC) {
        const char message[] =
            "my_free: corrupted block metadata\n";

        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    if (block->free) {
        const char message[] =
            "my_free: double free detected\n";

        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    block->free = 1;

    coalesce();
}

/* Allocate an array and initialize its memory to zero */
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

/* Resize an existing allocation */
void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return my_malloc(size);

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    if (size > SIZE_MAX - 15)
        return NULL;

    size_t need = align_up(size);

    if (need > SIZE_MAX - HDR)
        return NULL;

    /* Find the original block */
    block_t *block = head;

    while (block != NULL && payload(block) != ptr)
        block = block->next;

    if (block == NULL ||
        block->magic != MAGIC ||
        block->free)
        return NULL;

    size_t old_size = block->size;

    /* Case 1: Existing block is large enough */
    if (old_size >= need) {
        split_block(block, need);
        coalesce();
        return ptr;
    }

    /* Case 2: Expand into adjacent free blocks */
    while (block->next != NULL) {

        block_t *next = block->next;

        void *block_end =
            (char *)block + HDR + block->size;

        if (!next->free ||
            next->magic != MAGIC ||
            block_end != (void *)next)
            break;

        block->size += HDR + next->size;
        block->next = next->next;

        if (tail == next)
            tail = block;

        if (block->size >= need) {
            split_block(block, need);
            return ptr;
        }
    }

    /* Case 3: Allocate a new block and copy existing data */
    void *new_ptr = my_malloc(size);

    if (new_ptr == NULL)
        return NULL;

    size_t copy_size =
        old_size < size ? old_size : size;

    memcpy(new_ptr, ptr, copy_size);

    my_free(ptr);

    return new_ptr;
}

/* Collect heap usage statistics */
void heap_stats(heap_stats_t *out)
{
    if (out == NULL)
        return;

    out->heap_bytes = total_heap_bytes;
    out->used_bytes = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;

    block_t *current = head;

    while (current != NULL) {

        out->blocks++;

        if (current->free) {

            out->free_bytes += current->size;
            out->free_blocks++;

            if (current->size > out->largest_free)
                out->largest_free = current->size;
        }
        else {
            out->used_bytes += current->size;
        }

        current = current->next;
    }
}

/* Check allocator consistency */
int heap_check(void)
{
    if (head == NULL)
        return tail == NULL && total_heap_bytes == 0;

    if (tail == NULL)
        return 0;

    /* Detect linked-list cycles */
    block_t *slow = head;
    block_t *fast = head;

    while (fast != NULL && fast->next != NULL) {

        slow = slow->next;
        fast = fast->next->next;

        if (slow == fast)
            return 0;
    }

    block_t *current = head;
    block_t *last = NULL;

    while (current != NULL) {

        if (current->magic != MAGIC)
            return 0;

        if (current->free != 0 && current->free != 1)
            return 0;

        if ((uintptr_t)current % 16 != 0)
            return 0;

        if ((uintptr_t)payload(current) % 16 != 0)
            return 0;

        if (current->size % 16 != 0)
            return 0;

        if (current->next != NULL) {

            if ((uintptr_t)current->next <=
                (uintptr_t)current)
                return 0;
        }

        /* Adjacent free blocks should have been merged */
        if (current->next != NULL &&
            current->free &&
            current->next->free) {

            void *current_end =
                (char *)current + HDR + current->size;

            if (current_end == (void *)current->next)
                return 0;
        }

        last = current;
        current = current->next;
    }

    if (last != tail)
        return 0;

    if (tail->next != NULL)
        return 0;

    return 1;
}

/* Change allocation policy */
void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST)
        current_policy = p;
}

/* Display allocator block information */
void heap_dump(void)
{
    char buffer[256];

    int length = snprintf(
        buffer,
        sizeof(buffer),
        "\n========== HEAP DUMP ==========\n"
    );

    if (length > 0)
        write(STDOUT_FILENO, buffer, (size_t)length);

    block_t *current = head;
    size_t index = 0;

    while (current != NULL) {

        length = snprintf(
            buffer,
            sizeof(buffer),
            "Block %zu | Address: %p | Size: %zu | Status: %s\n",
            index,
            (void *)current,
            current->size,
            current->free ? "FREE" : "USED"
        );

        if (length > 0)
            write(STDOUT_FILENO, buffer, (size_t)length);

        current = current->next;
        index++;
    }

    length = snprintf(
        buffer,
        sizeof(buffer),
        "Total heap bytes: %zu\n"
        "Total blocks: %zu\n"
        "===============================\n",
        total_heap_bytes,
        index
    );

    if (length > 0)
        write(STDOUT_FILENO, buffer, (size_t)length);
}
