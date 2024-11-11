#ifndef GLAD_H
#define GLAD_H

#include <stddef.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>

 /* 
  * goals: 
  * 	- aligned/unaliged allocations 
  * 	- memory profiling 
  * 	- debugging, pass in info via a macro 
  * 	- support for new backends? WASM?
  */ 

/* appropriately large. in this case, a full PMD (the layer of the 
 * page table hierarchy above a PTE) work of pages on x86 */
#define DEFAULT_CHUNK_SIZE (4096L*1024L)/(sizeof(char))
#define CHUNK_ALLOC_SIZE(X) (sizeof(chunk) + (sizeof(char) * X))

#define ROUND_UP(val, size) ((val + size - 1) & -size)
#define ZEROMEM 0x1 	/* If this flag is set for a function that accepts it, the corresponding memory will be zeroed */
#define SOFTFAIL 0x10	/* If this flag is set, the function will return an error value instead of assert(0). */

#define glad_new(...) glad_new_impl(__VA_ARGS__, glad_new4, glad_new3, glad_new2)(__VA_ARGS__)
#define glad_new_impl(_1, _2, _3, _4, FUNC, ...) FUNC
#define glad_new2(a, t)          (t *)arena_alloc(a, sizeof(t), alignof(t), ZEROMEM)
#define glad_new3(a, t, n)       (t *)arena_alloc(a, n * sizeof(t), alignof(t), ZEROMEM)
#define glad_new4(a, t, n, f)    (t *)arena_alloc(a, n * sizeof(t), alignof(t), f)

#define glad_push(...) glad_push_impl(__VA_ARGS__, glad_push5, glad_push4)(__VA_ARGS__)
#define glad_push_impl(_1, _2, _3, _4, _5, FUNC, ...) FUNC
#define glad_push4(a, t, d, n)    (t *)arena_push(a, d, n * sizeof(t), alignof(t), ZEROMEM)
#define glad_push5(a, t, d, n, f) (t *)arena_push(a, d, n * sizeof(t), alignof(t), f)


typedef struct chunk chunk;
struct chunk {
	chunk* ch_next;
	ptrdiff_t ch_size;
	ptrdiff_t ch_offset;
	char ch_data[];
};

typedef struct arena arena; 
struct arena {
	chunk* ar_head;
	chunk* ar_tail;
};

/** 
 * @brief 	Allocates a single chunk with at least the specified size.
 * 
 * @details
 * 		Uses mmap to allocate a chunk of memory with at least enough room
 *		for the chunk struct itself and size*sizeof(char) bytes.
 *
 * @param size 	The minimum number of bytes to allocate in the chunk.
 * @param flags Flags that modify allocation behavior, e.g., `ZEROMEM` for zeroing memory.
 *
 * @return 	A pointer to the allocated chunk. If mmap fails for any reason, this
 * 		function returns 0.
 */
chunk* alloc_chunk(ptrdiff_t size, int flags)
{
	if (!size)
		return 0;

	ptrdiff_t allocation_size = CHUNK_ALLOC_SIZE(size);
	
	if (allocation_size < size)
		return 0;

	/* Anonymous map, compatible with systems that lack MAP_ANONYMOUS */	
	int fd = open("/dev/zero", O_RDWR);
	
	if (fd == -1)
		return 0;

	chunk* ret_chunk = (chunk*)mmap(0,
			allocation_size, 
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE,
			fd, 0);
	close(fd);
	if (ret_chunk == MAP_FAILED) {
		if (flags & SOFTFAIL) {
			return 0;	
		}
		assert(0);
	}

	if (flags & ZEROMEM) {
		memset(ret_chunk, 0, allocation_size);
	}

	ret_chunk->ch_next = 0;
	ret_chunk->ch_size = size;
	ret_chunk->ch_offset = 0;
	return ret_chunk;
}

/** 
 * @brief 	Frees the chunk at the provided reference.
 *
 * @param ch 	Pointer to the chunk to be freed.
 *
 * @return  	void, asserts on munmap failure.
 */
void free_chunk(chunk* chunk)
{
	if (!chunk) return;
	ptrdiff_t allocation_size = CHUNK_ALLOC_SIZE(chunk->ch_size);
	int check = munmap(chunk, allocation_size);	
	assert(!check);
}

/** 
 * @brief 	Allocates num_bytes in the given arena.
 * 
 * @details
 * 		Uses mmap to allocate a chunk of memory with at least enough room
 * 		for the chunk struct itself and size*sizeof(char) bytes.
 *
 * @param arena 	The arena in which memory will be allocated.
 * @param num_bytes Number of bytes to allocate.
 * @param flags 	Flags that modify allocation behavior, e.g., `ZEROMEM` for zeroing memory.
 
 * @return 	A pointer to the start of the allocated memory region. Null if the 
 * 		allocation fails. 
 */
void* arena_alloc(arena* arena, const ptrdiff_t num_bytes, const ptrdiff_t alignment, int flags)
{
    if (!arena || !num_bytes || !alignment || alignment & (alignment - 1))
		// alignment must be non-zero, a power of 2, and num_bytes > 0
        return 0;

    // Calculate the aligned size for allocation
    ptrdiff_t alloc_size = ROUND_UP(num_bytes, alignment);
	ptrdiff_t chunk_size = ROUND_UP(alloc_size, DEFAULT_CHUNK_SIZE);
    void* start_addr = 0;

    // Initialize arena if empty
    if (!arena->ar_tail) {
        assert(!arena->ar_head);
        chunk* new_head = alloc_chunk(chunk_size, flags);
        if (!new_head) return 0;
        arena->ar_head = new_head;
        arena->ar_tail = arena->ar_head;
    }

    // Find a chunk that can accommodate the allocation
    chunk* cursor = arena->ar_head;
    while (cursor) {
        void* current_address = &cursor->ch_data[cursor->ch_offset];
        uintptr_t misalignment = (uintptr_t)current_address % alignment;

        // Adjust the offset if the address is misaligned
        if (misalignment) {
            cursor->ch_offset += alignment - misalignment;
        }

        if (cursor->ch_size - cursor->ch_offset >= alloc_size) {
            start_addr = &cursor->ch_data[cursor->ch_offset];
            cursor->ch_offset += alloc_size;
            return start_addr;
        }
        cursor = cursor->ch_next;
    }

    // Allocate a new chunk if needed and align start address
    chunk* new_chunk = alloc_chunk(chunk_size, flags);
    if (!new_chunk) return 0;

    // Ensure new chunk start address is aligned
	void* current_address = new_chunk->ch_data;
	uintptr_t misalignment = (uintptr_t)current_address % alignment;

	// Adjust the offset if the address is misaligned
	if (misalignment) {
		new_chunk->ch_offset += alignment - misalignment;
	}
    start_addr = &new_chunk->ch_data[new_chunk->ch_offset];
    new_chunk->ch_offset += alloc_size;

    // Link new chunk to arena
    arena->ar_tail->ch_next = new_chunk;
    arena->ar_tail = new_chunk;
    return start_addr;
}

/** 
 * @brief	Gets the total in-use size of the arena.
 * 
 * @details
 * 		Size is measured in (sizeof char).
 *
 * @param arena The arena for which to calculate the in-use size.
 *
 * @return 	The computed size.
 */
ptrdiff_t arena_get_size(arena* arena)
{
	if (!arena)
		return 0;

	chunk* cursor = arena->ar_head;
	ptrdiff_t size = 0;
	while (cursor) {
		size += cursor->ch_offset;
		cursor = cursor->ch_next;	
	}

	return size;
};


/** 
 * @brief	Pushes the buffer onto the arena.
 * 
 * @param arena The arena to which data will be pushed.
 * @param data 	Pointer to the data buffer to push onto the arena.
 * @param size 	Number of bytes in the data buffer.
 * @param flags Flags that modify allocation behavior, e.g., `ZEROMEM` for zeroing memory.
 *
 * @return 	A pointer to the start of the newly allocated region.
 */
void* arena_push(arena* arena, void* data, ptrdiff_t size, ptrdiff_t alignment, int flags)
{
	if (!arena || !data) 
		return 0;

	void* start_addr = arena_alloc(arena, size, alignment, flags);
	if (!start_addr) return 0;
	memcpy(start_addr, data, size);
	return start_addr;
}

/** 
 * @brief	Crops the arena to its final in-use element and coalesces the chunks.
 * 
 * @details
 * 		This coalescing resolves internal fragmentation of each chunk by
 * 		copying over only the allocated portions.
 *
 * @param arena The arena to crop and coalesce.
 * @param flags Flags that modify behavior, e.g., `ZEROMEM` for zeroing memory.
 *
 * @return 	A pointer to the start of the newly allocated region.
 */
void* arena_crop_and_coalesce(arena* arena, int flags)
{
	if (!arena)
		return 0;	

	ptrdiff_t alloc_size = arena_get_size(arena);
	chunk* cropped = alloc_chunk(alloc_size, flags); 

	if (!cropped)
		return 0;

	chunk* cursor = arena->ar_head;
	ptrdiff_t curr_offset = 0;
	while (cursor) {
		memcpy(&cropped->ch_data[curr_offset],
				cursor->ch_data,
				sizeof(char) * cursor->ch_offset);
		curr_offset += cursor->ch_offset;
		chunk* prev = cursor;
		cursor = cursor->ch_next;	
		free_chunk(prev);
	}
		
	arena->ar_head = cropped;
	arena->ar_tail = arena->ar_head;
	return cropped->ch_data;
}

/** 
 * @brief 	Clears the given arena without freeing the memory.
 *
 * @details
 *		Marks the chunks as empty again so that they may be used.
 *
 * @param arena The arena to clear.
 */
void arena_clear(arena* arena)
{
	if (!arena)
		return;

	chunk* cursor = arena->ar_head;
	while (cursor) {
		memset(cursor->ch_data, 0, sizeof(char) * cursor->ch_size);
		cursor->ch_offset = 0;
		cursor = cursor->ch_next;	
	}
}

/** 
 * @brief 	Marks whole arena as unallocated.
 *
 * @details
 * 		Does not set the associated memory. Use with care.
 *
 * @param arena The arena to reset.
 */
void arena_reset(arena* arena)
{
	if (!arena)
		return;

	chunk* cursor = arena->ar_head;
	while (cursor) {
		cursor->ch_offset = 0;
		cursor = cursor->ch_next;	
	}
}

/** 
 * @brief  Frees the given arena.
 *
 * @param arena The arena to free.
 * @param flags Flags that modify free behavior, e.g., `ZEROMEM` for zeroing memory.
 */
void arena_free(arena* arena, int flags)
{
	if (!arena)
		return;

	chunk* cursor = arena->ar_head;
	while (cursor) {
		if (flags & ZEROMEM) {
			memset(cursor, 0, CHUNK_ALLOC_SIZE(cursor->ch_size));
		}
		chunk* prev = cursor;
		cursor = cursor->ch_next;	
		free_chunk(prev);
	}
}	

/** 
 * @brief	Copies arena copy_src to copy_dst.
 * 
 * @details
 * 		Any allocation failure results in cleanup.
 * 
 * @param copy_dst 	The destination arena to which data will be copied.
 * @param copy_src 	The source arena from which data will be copied.
 * @param flags 	Flags that modify allocation behavior, e.g., `ZEROMEM` for zeroing memory.
 */
void arena_copy(arena *restrict copy_dst, const arena *restrict copy_src, int flags)
{
	if (!copy_dst || !copy_src) 
		return;
	if (!copy_src->ar_head || !copy_src->ar_tail)
		return;
	
	/* allocate the head of our new list */	
	chunk *dst_head = alloc_chunk(copy_src->ar_head->ch_size, flags);
	dst_head->ch_offset = copy_src->ar_head->ch_offset;
	dst_head->ch_size = copy_src->ar_head->ch_size;
	memcpy(dst_head->ch_data,
			copy_src->ar_head->ch_data, 
			sizeof(char) * copy_src->ar_head->ch_offset);
	copy_dst->ar_head = dst_head;
	copy_dst->ar_tail = copy_dst->ar_head;

	chunk *src_cursor = copy_src->ar_head->ch_next;
	chunk *dst_cursor = dst_head;
	while (src_cursor) {
		chunk *new_chunk = alloc_chunk(src_cursor->ch_size, flags);
		/* cleanup the new area if we ever fail to allocate */
		if (!new_chunk) {
		    arena_free(copy_dst, flags);
		    return;
		}
		
		new_chunk->ch_offset = src_cursor->ch_offset;
		new_chunk->ch_size = src_cursor->ch_size;
		memcpy(new_chunk->ch_data,
				src_cursor->ch_data,
				sizeof(char) * src_cursor->ch_offset);
		
		dst_cursor->ch_next = new_chunk;

		copy_dst->ar_tail = new_chunk;
		dst_cursor = new_chunk;
		src_cursor = src_cursor->ch_next;
	}
}
#endif /* GLAD_H */
