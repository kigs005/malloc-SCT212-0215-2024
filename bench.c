#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "myalloc.h"

#define OPERATIONS 50000
#define SLOTS 1024
#define MAX_SIZE 4096
#define SEED UINT32_C(123456789)

typedef enum {
    MODE_FIRST,
    MODE_BEST,
    MODE_LIBC
} bench_mode_t;

static uint32_t random_state = SEED;
static void *slots[SLOTS];

static uint32_t next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;

    return random_state;
}

static double elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static void run_benchmark(bench_mode_t mode)
{
    random_state = SEED;

    for (size_t i = 0; i < SLOTS; i++)
        slots[i] = NULL;

    if (mode == MODE_FIRST)
        set_policy(FIT_FIRST);
    else if (mode == MODE_BEST)
        set_policy(FIT_BEST);

    struct timespec start;
    struct timespec end;

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        _exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < OPERATIONS; i++) {
        size_t index = next_random() % SLOTS;

        if (slots[index] != NULL) {
            if (mode == MODE_LIBC)
                free(slots[index]);
            else
                my_free(slots[index]);

            slots[index] = NULL;
        } else {
            size_t size = (next_random() % MAX_SIZE) + 1;

            if (mode == MODE_LIBC)
                slots[index] = malloc(size);
            else
                slots[index] = my_malloc(size);
        }
    }

    for (size_t i = 0; i < SLOTS; i++) {
        if (slots[i] != NULL) {
            if (mode == MODE_LIBC)
                free(slots[i]);
            else
                my_free(slots[i]);

            slots[i] = NULL;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        _exit(EXIT_FAILURE);
    }

    double elapsed = elapsed_seconds(start, end);

    if (mode == MODE_LIBC) {
        printf("Allocator: libc malloc/free\n");
        printf("Operations: %d\n", OPERATIONS);
        printf("Elapsed time: %.6f seconds\n", elapsed);
        printf("Custom heap statistics: not applicable\n\n");
    } else {
        heap_stats_t stats;
        heap_stats(&stats);

        double fragmentation = 0.0;

        if (stats.free_bytes > 0) {
            fragmentation =
                1.0 - ((double)stats.largest_free / stats.free_bytes);
        }

        printf("Allocator: %s\n",
               mode == MODE_FIRST ? "First-fit" : "Best-fit");
        printf("Operations: %d\n", OPERATIONS);
        printf("Elapsed time: %.6f seconds\n", elapsed);
        printf("Heap bytes: %zu\n", stats.heap_bytes);
        printf("Used bytes: %zu\n", stats.used_bytes);
        printf("Free bytes: %zu\n", stats.free_bytes);
        printf("Total blocks: %zu\n", stats.blocks);
        printf("Free blocks: %zu\n", stats.free_blocks);
        printf("Largest free block: %zu\n", stats.largest_free);
        printf("External fragmentation: %.4f (%.2f%%)\n\n",
               fragmentation, fragmentation * 100.0);

        if (!heap_check()) {
            fprintf(stderr, "Heap consistency check failed\n");
            _exit(EXIT_FAILURE);
        }
    }
}

static int run_isolated(bench_mode_t mode)
{
    fflush(stdout);

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child == 0) {
        run_benchmark(mode);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    }

    int status;

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int main(void)
{
    printf("Custom Allocator Benchmark\n");
    printf("==========================\n");
    printf("Operations: %d\n", OPERATIONS);
    printf("Allocation sizes: 1-%d bytes\n", MAX_SIZE);
    printf("Pointer slots: %d\n", SLOTS);
    printf("Random seed: %u\n\n", SEED);

    if (run_isolated(MODE_FIRST) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    if (run_isolated(MODE_BEST) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    if (run_isolated(MODE_LIBC) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
