#ifndef GLAD_H
#define GLAD_H

#include <stddef.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

 /* 
  * goals: 
  * 	- aligned/unaliged allocations 
  * 	- memory profiling 
  * 	- debugging, pass in info via a macro 
  * 	- support for new backends? WASM?
  */ 

/* appropriately large. in this case, a full PMD (the layer of the 
 * page table hierarchy above a PTE) work of pages on x86 */
#define DEFAULT_CHUNK_SIZE (4096L*1024L)/(sizeof(char*))
#define CHUNK_ALLOC_SIZE(X) (sizeof(chunk) + (sizeof(char*) * X)) 

#define ROUND_UP(val, size) ((val + size - 1) & -size);
#define ZEROMEM 0x1
#define SOFTFAIL 0x10

#define RETURN_ERRVAL_ON_ERR(val, errval, fail_err, new_errno) 	\
if (val == fail_err) {  										\
		errno = new_errno;										\
		return errval;											\
}
#define RETURN_ON_ERR(val, fail_err, new_errno) RETURN_ERRVAL_ON_ERR(val, fail_err, fail_err, new_errno) 
#define RETURN_NULL_ON_ERR(val, fail_err, new_errno) RETURN_ERRVAL_ON_ERR(val, 0, fail_err, new_errno)

typedef struct chunk chunk;
struct chunk {
	chunk* ch_next;
	ptrdiff_t ch_size;
	ptrdiff_t ch_offset;
	char *ch_data[];
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
 * 			Uses mmap to allocate a chunk of memory with at least enough room
 * 			for the chunk struct itself and size*sizeof(char*) bytes. 
 *
 * @return 	A pointer to the allocated chunk. If mmap fails for any reason, this
 * 			function returns 0.
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

	chunk* ret_chunk = mmap(0, 
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
 * @return  void, asserts on munmap failure	
 *
 */
void free_chunk(chunk* ch)
{
	assert(ch);
	ptrdiff_t allocation_size = CHUNK_ALLOC_SIZE(ch->ch_size);
	int check = munmap(ch, allocation_size);	
	assert(!check);
}

/* Basic arena operations */

/** 
 * @brief 	Allocates num_bytes in the given arena	
 * 
 * @details
 * 			Uses mmap to allocate a chunk of memory with at least enough room
 * 			for the chunk struct itself and size*sizeof(char*) bytes. 
 *
 * @return 	A pointer to the start of the allocated memory region. Null if the 
 * 			allocation fails. 
 */
void* arena_alloc(arena* arena, const ptrdiff_t num_bytes, int flags)
{
	if (!arena)
		return 0;	

	ptrdiff_t alloc_size = ROUND_UP(num_bytes, sizeof(char*));
	void* start_addr = 0; 

	/* if our arena is empty */
	if (!arena->ar_tail) {
		assert(!arena->ar_head); /* Something is very broken if we have start but lost end */
		ptrdiff_t chunk_size = (alloc_size >= DEFAULT_CHUNK_SIZE)
			?  (alloc_size * 2): DEFAULT_CHUNK_SIZE;
		chunk* new_head = alloc_chunk(chunk_size, flags);
		if (!new_chunk) return 0;
		arena->ar_head = new_head;
		arena->ar_tail = arena->ar_head;
	}

	/* find the next chunk that will fit our allocation */
	chunk* cursor = arena->ar_head;
	while (cursor) {
		if (cursor->ch_size - cursor->ch_offset >= alloc_size) {
			start_addr = &cursor->ch_data[cursor->ch_offset];
			cursor->ch_offset += alloc_size;
			return start_addr;
		}	
		cursor = cursor->ch_next;	
	}

	/* must allocate a new chunk to the arena */
	assert(!cursor);
	ptrdiff_t chunk_size = (alloc_size * 2);
	chunk* new_chunk = alloc_chunk(chunk_size, flags);
	if (!new_chunk) return 0;
	start_addr = new_chunk->ch_data;
	arena->ar_tail->ch_next = new_chunk;
	arena->ar_tail = new_chunk;
	return start_addr; 
}

/** 
 * @brief	Gets the total in-use size of the arena	
 * 
 * @details
 * 			Size is measured in (sizeof char*).
 *
 * @return 	The computed size.	
 */
ptrdiff_t arena_get_size(arena* arena) {
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
 * @brief	Pushes the buffer onto the arena	
 * 
 * @return 	A pointer to the start of the newly allocated region.	
 */
void* arena_push(arena* arena, void* data, ptrdiff_t size, int flags) {
	if (!arena || !data) 
		return 0;

	void* start_addr = arena_alloc(arena, size, flags); 
	memcpy(start_addr, data, size);
	return start_addr;
}

/** 
 * @brief	Crops the arena to its final in-use element and coalesces the chunks	
 * 
 * @details
 * 			This coalescing resolves internal fragmentation of each chunk by
 * 			copying over only the allocated portions.
 *
 * @return 	A new pointer to the final in-use element.	
 */
void* arena_crop_and_coalesce(arena* arena, int flags)
{
	if (!arena)
		return 0;	

	ptrdiff_t alloc_size = arena_get_size(arena);
	chunk* cropped = alloc_chunk(alloc_size, flags); 

	chunk* cursor = arena->ar_head;
	ptrdiff_t curr_offset = 0;
	while (cursor) {
		memcpy(&cropped->ch_data[curr_offset],
				cursor->ch_data,
				sizeof(char*) * cursor->ch_offset);
		curr_offset += cursor->ch_offset;
		chunk* prev = cursor;
		cursor = cursor->ch_next;	
		free_chunk(prev);
	}
		
	arena->ar_head = cropped;
	arena->ar_tail = arena->ar_head;
	return &cropped->ch_data[curr_offset];
}

/** 
 * @brief	Copies arena copy_src to cpy_dst.	
 * 
 * @details
 * 		Any allocation failure results in cleanup.
 */
void arena_copy(arena *restrict copy_dst, const arena *restrict copy_src)
{
	if (!copy_dst || !copy_src) 
		return;

	copy_dst->ar_head = 0;
	copy_dst->ar_tail = 0;
	
	chunk *src_cursor = copy_src->ar_head;
	chunk *dst_cursor = 0;
	
	while (src_cursor) {
		chunk *new_chunk = alloc_chunk(src_cursor->ch_size, 0);
		/* cleanup the new area if we ever fail to allocate */
		if (!new_chunk) {
		    arena_free(copy_dst, ZEROMEM);
		    return;
		}
		
		new_chunk->ch_offset = src_cursor->ch_offset;
		memcpy(new_chunk->ch_data, src_cursor->ch_data, sizeof(char*) * src_cursor->ch_offset);
		
		if (!copy_dst->ar_head) {
		    copy_dst->ar_head = new_chunk;
		} else {
		    dst_cursor->ch_next = new_chunk;
		}
		
		copy_dst->ar_tail = new_chunk;
		dst_cursor = new_chunk;
		src_cursor = src_cursor->ch_next;
	}
}

/** 
 * @brief 	Clears the given arena without freeing the memory 
 *
 * @details
 *			Marks the chunks as empty again so that they may be used
 */
void arena_clear(arena* arena) {
	if (!arena)
		return;

	chunk* cursor = arena->ar_head;
	while (cursor) {
		memset(cursor->ch_data, 0, sizeof(char*) * cursor->ch_size);
		cursor->ch_offset = 0;
		cursor = cursor->ch_next;	
	}
}

/** 
 * @brief 	Marks whole arena as unallocated.	
 *
 * @details
 * 			Does not set the associated memory. Use with care.
 */
void arena_reset(arena* arena) {
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
 */
void arena_free(arena* arena, int flags) {
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
#endif /* GLAD_H */
