#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "glad.h"

// Helper function for running individual tests
void run_test(const char* test_name, void (*test_func)()) {
    printf("Running %s...\n", test_name);
    test_func();
    printf("%s passed.\n", test_name);
}

// Test alloc_chunk with basic, zero size, and error cases
void test_alloc_chunk_basic() {
    chunk* ch = alloc_chunk(1024, 0);
    assert(ch);
    assert(ch->ch_size == 1024);
    assert(ch->ch_offset == 0);
    free_chunk(ch);
}

void test_alloc_chunk_zero_size() {
    chunk* ch = alloc_chunk(0, 0);
    assert(ch == 0);
}

void test_alloc_chunk_zeromem() {
    chunk* ch = alloc_chunk(512, ZEROMEM);
    assert(ch);
	unsigned char* data = (unsigned char*)ch->ch_data;
    for (size_t i = 0; i < 512; ++i) {
        assert(data[i] == 0);
    }
    free_chunk(ch);
}

// Test free_chunk for valid and null inputs
void test_free_chunk_valid() {
    chunk* ch = alloc_chunk(512, 0);
    assert(ch);
    free_chunk(ch);  // Should not crash
}

void test_free_chunk_null() {
    // Pass 0 to free_chunk, it should not fail/assert
    free_chunk(0);
}

// Test arena_alloc with valid, zero size, and large allocation
void test_arena_alloc_basic() {
    arena ar = {0};
    void* mem = arena_alloc(&ar, 1024, alignof(char), 0);
    assert(mem);
    assert(ar.ar_head);
    arena_free(&ar, 0);
}

void test_arena_alloc_zero_size() {
    arena ar = {0};
    void* mem = arena_alloc(&ar, 0, alignof(char), 0);
    assert(mem == 0);
    arena_free(&ar, 0);
}

void test_arena_alloc_large_size() {
    arena ar = {0};
    void* mem = arena_alloc(&ar, DEFAULT_CHUNK_SIZE * 2, alignof(char), 0);
    assert(mem);
    assert(ar.ar_head);
    arena_free(&ar, 0);
}

// Test arena_get_size for empty and large arenas
void test_arena_get_size_empty() {
    arena ar = {0};
    assert(arena_get_size(&ar) == 0);
}

void test_arena_get_size_partial() {
    arena ar = {0};
    arena_alloc(&ar, 128, alignof(char), 0);
    assert(arena_get_size(&ar) >= 128);
    arena_free(&ar, 0);
}

void test_arena_get_size_multiple_chunks() {
    arena ar = {0};
    arena_alloc(&ar, DEFAULT_CHUNK_SIZE, alignof(char), 0);
    arena_alloc(&ar, DEFAULT_CHUNK_SIZE, alignof(char), 0);
    assert(arena_get_size(&ar) >= DEFAULT_CHUNK_SIZE * 2);
    arena_free(&ar, 0);
}

// Test arena_push with normal, large, and null data
void test_arena_push_basic() {
    arena ar = {0};
    int data = 42;
    int* pushed_data = (int*)arena_push(&ar, &data, sizeof data, alignof(int), 0);
    assert(pushed_data);
    assert(*pushed_data == 42);
    arena_free(&ar, 0);
}

void test_arena_push_null_data() {
    arena ar = {0};
    void* result = arena_push(&ar, 0, 4, 4, 0);
    assert(result == 0);
    arena_free(&ar, 0);
}

void test_arena_push_large_data() {
    arena ar = {0};
    char data[DEFAULT_CHUNK_SIZE];
    memset(data, 1, DEFAULT_CHUNK_SIZE);
    char* pushed_data = (char*)arena_push(&ar, data, sizeof data, alignof(char), 0);
    assert(pushed_data);
    for (size_t i = 0; i < sizeof(data); ++i) {
        assert(pushed_data[i] == 1);
    }
    arena_free(&ar, 0);
}

// Test arena_crop_and_coalesce with edge cases and verify content
void test_arena_crop_and_coalesce_empty() {
    arena ar = {0};
    void* result = arena_crop_and_coalesce(&ar, 0);
    assert(result == 0);
}

void test_arena_crop_and_coalesce_basic() {
    arena ar = {0};
    int data = 42;
    int* pushed_data = (int*) arena_push(&ar, &data, sizeof data, alignof(int), 0);
    int* coalesced_data = (int*) arena_crop_and_coalesce(&ar, 0);
	assert(pushed_data);
    assert(coalesced_data);
    assert(*coalesced_data == 42);
    arena_free(&ar, 0);
}

void test_arena_crop_and_coalesce_large() {
    arena ar = {0};
    char data[DEFAULT_CHUNK_SIZE];
    memset(data, 7, sizeof(data));
    arena_push(&ar, data, sizeof data, alignof(char), 0);
    arena_push(&ar, data, sizeof data, alignof(char), 0);
    arena_push(&ar, data, sizeof data, alignof(char), 0);
    char* coalesced_data = (char*) arena_crop_and_coalesce(&ar, ZEROMEM);
    assert(coalesced_data);
    for (size_t i = 0; i < sizeof(data); ++i) {
        assert(coalesced_data[i] == 7);
    }
    arena_free(&ar, 0);
}

// Test arena_copy with valid and empty arenas
void test_arena_copy_basic() {
    arena src = {0}, dst = {0};
    int data = 123;
    arena_push(&src, &data, sizeof data, alignof(int), 0);
    arena_copy(&dst, &src, 0);
    int* dst_data = (int*)dst.ar_head->ch_data;
    assert(dst_data && *dst_data == 123);
    arena_free(&src, 0);
    arena_free(&dst, 0);
}

void test_arena_copy_empty() {
    arena src = {0}, dst = {0};
    arena_copy(&dst, &src, 0);
    assert(arena_get_size(&dst) == 0);
    arena_free(&dst, 0);
}

// Test arena_clear to ensure all offsets are reset
void test_arena_clear() {
    arena ar = {0};
    arena_alloc(&ar, 128, alignof(char), 0);
    arena_clear(&ar);
    assert(arena_get_size(&ar) == 0);
    arena_free(&ar, 0);
}

// Test arena_reset with edge and normal cases
void test_arena_reset() {
    arena ar = {0};
    arena_alloc(&ar, 128, alignof(char), 0);
    arena_reset(&ar);
    assert(ar.ar_head->ch_offset == 0);
    arena_free(&ar, 0);
}

// Test arena_free for valid and empty arenas
void test_arena_free_basic() {
    arena ar = {0};
    arena_alloc(&ar, 128, alignof(char), ZEROMEM);
    arena_free(&ar, ZEROMEM); // Should not crash
}

void test_arena_free_empty() {
    arena ar = {0};
    arena_free(&ar, ZEROMEM); // Should not crash
}

void test_arena_alloc_alignment_basic() {
    arena ar = {0};

    // Test allocation with basic alignment (4-byte boundary)
    int* data = (int*)arena_alloc(&ar, sizeof(int), alignof(int), 0);
    assert(data);
    assert(((uintptr_t)data % alignof(int)) == 0);

    // Test allocation with 16-byte alignment
    int* aligned_data = (int*)arena_alloc(&ar, sizeof(int), 16, 0);
    assert(aligned_data);
    assert(((uintptr_t)aligned_data % 16) == 0);

    arena_free(&ar, 0);
}

void test_arena_alloc_large_alignment() {
    arena ar = {0};

    // Allocate with a large alignment (64-byte boundary)
    void* data = arena_alloc(&ar, 64, 64, 0);
    assert(data);
    assert(((uintptr_t)data % 64) == 0);

    // Test another large aligned allocation that exceeds DEFAULT_CHUNK_SIZE
    void* large_data = arena_alloc(&ar, DEFAULT_CHUNK_SIZE * 2, 128, 0);
    assert(large_data);
    assert(((uintptr_t)large_data % 128) == 0);

    arena_free(&ar, 0);
}

void test_arena_push_aligned_data() {
    arena ar = {0};
    char data[128] = {0};
    memset(data, 1, sizeof(data));

    // Test pushing data with specific alignment
    char* pushed_data = (char*)arena_push(&ar, data, sizeof(data), alignof(int), 0);
    assert(pushed_data);
    assert(((uintptr_t)pushed_data % alignof(int)) == 0);

    // Check if the data matches the original content
    for (size_t i = 0; i < sizeof(data); ++i) {
        assert(pushed_data[i] == 1);
    }

    arena_free(&ar, 0);
}

void test_arena_alloc_alignment_zero_size() {
    arena ar = {0};

    // Test zero-size allocation with alignment, expect null
    void* data = arena_alloc(&ar, 0, alignof(int), 0);
    assert(!data);

    arena_free(&ar, 0);
}

void test_arena_alloc_alignment_edge_cases() {
    arena ar = {0};

    // Test allocation where size is not a multiple of alignment
    void* data1 = arena_alloc(&ar, 7, 8, 0); // 7 bytes with 8-byte alignment
    assert(data1);
    assert(((uintptr_t)data1 % 8) == 0);

    // Test allocation with very large alignment requirement
    void* data2 = arena_alloc(&ar, 16, 256, 0);
    assert(data2);
    assert(((uintptr_t)data2 % 256) == 0);

    arena_free(&ar, 0);
}

void test_arena_alloc_alignment_fallback_chunk_allocation() {
    arena ar = {0};

    // Test allocation larger than current chunk, forces a new chunk allocation
    void* data1 = arena_alloc(&ar, DEFAULT_CHUNK_SIZE / 2, 32, 0);
    assert(data1);
    assert(((uintptr_t)data1 % 32) == 0);

    void* data2 = arena_alloc(&ar, DEFAULT_CHUNK_SIZE, 64, 0); // This should force a new chunk
    assert(data2);
    assert(((uintptr_t)data2 % 64) == 0);

    arena_free(&ar, 0);
}


// Main function to run all tests
int main() {
    run_test("test_alloc_chunk_basic", test_alloc_chunk_basic);
    run_test("test_alloc_chunk_zero_size", test_alloc_chunk_zero_size);
    run_test("test_alloc_chunk_zeromem", test_alloc_chunk_zeromem);

    run_test("test_free_chunk_valid", test_free_chunk_valid);
    run_test("test_free_chunk_null", test_free_chunk_null);

    run_test("test_arena_alloc_basic", test_arena_alloc_basic);
    run_test("test_arena_alloc_zero_size", test_arena_alloc_zero_size);
    run_test("test_arena_alloc_large_size", test_arena_alloc_large_size);

    run_test("test_arena_get_size_empty", test_arena_get_size_empty);
    run_test("test_arena_get_size_partial", test_arena_get_size_partial);
    run_test("test_arena_get_size_multiple_chunks", test_arena_get_size_multiple_chunks);

    run_test("test_arena_push_basic", test_arena_push_basic);
    run_test("test_arena_push_null_data", test_arena_push_null_data);
    run_test("test_arena_push_large_data", test_arena_push_large_data);

    run_test("test_arena_crop_and_coalesce_empty", test_arena_crop_and_coalesce_empty);
    run_test("test_arena_crop_and_coalesce_basic", test_arena_crop_and_coalesce_basic);
    run_test("test_arena_crop_and_coalesce_large", test_arena_crop_and_coalesce_large);

    run_test("test_arena_copy_basic", test_arena_copy_basic);
    run_test("test_arena_copy_empty", test_arena_copy_empty);

    run_test("test_arena_clear", test_arena_clear);
    run_test("test_arena_reset", test_arena_reset);

    run_test("test_arena_free_basic", test_arena_free_basic);
	run_test("test_arnea_free_empty", test_arena_free_empty);

    run_test("test_arena_alloc_alignment_basic", test_arena_alloc_alignment_basic);
    run_test("test_arena_alloc_large_alignment", test_arena_alloc_large_alignment);
    run_test("test_arena_push_aligned_data", test_arena_push_aligned_data);
    run_test("test_arena_alloc_alignment_zero_size", test_arena_alloc_alignment_zero_size);
    run_test("test_arena_alloc_alignment_edge_cases", test_arena_alloc_alignment_edge_cases);
    run_test("test_arena_alloc_alignment_fallback_chunk_allocation", test_arena_alloc_alignment_fallback_chunk_allocation);

    printf("All tests passed.\n");
    return 0;
}
