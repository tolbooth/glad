#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "glad.h" // Assuming arena_alloc and arena_push are defined here

#define ARRAY_SIZE (128) 
#define NUM_ITERATIONS 1000000


void benchmark_malloc() {
    clock_t start, end;
    void* mem;

    start = clock();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        mem = malloc(ARRAY_SIZE); // Use malloc for allocation
        if (!mem) {
            perror("malloc failed");
            exit(1);
        }
        memset(mem, i, ARRAY_SIZE); // Optionally memset to simulate usage
		int x = ((int*)mem)[127];
        free(mem); // Free memory after use
    }
    end = clock();

    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("malloc: Total time for %d iterations: %f seconds\n", NUM_ITERATIONS, time_spent);
}

void benchmark_arena_alloc() {
    clock_t start, end;
    arena ar = {0}; // Initialize the arena
    void* mem;

    start = clock();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        mem = glad_new(&ar, char, ARRAY_SIZE, 0); // Use arena_alloc
        if (!mem) {
            perror("arena_alloc failed");
            exit(1);
        }
        memset(mem, i, ARRAY_SIZE); // Optionally memset to simulate usage
		int x = ((int*)mem)[127];
    }
    arena_free(&ar, 0);
    end = clock();

    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("arena_alloc: Total time for %d iterations: %f seconds\n", NUM_ITERATIONS, time_spent);
}

int main() {
    printf("Benchmarking malloc vs. arena_alloc...\n");

    benchmark_malloc();        // Run malloc benchmark
    benchmark_arena_alloc();   // Run arena_alloc benchmark

    return 0;
}
