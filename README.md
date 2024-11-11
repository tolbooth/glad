# Glad - An Arena Allocator

Glad is a small, single header arena allocator backed by `mmap` for use on UNIX systems.

## Usage
To use Glad in your project, clone the repository and include the header `glad.h` in your code. Compile your project with any C99-compliant compiler. Its main usecase is in handling data with tightly coupled lifetimes, that may be inconvenient to handle with `malloc`/`free`. Throw a tree on the arena, and throw it all away when you're done!


```c
#include "glad.h"

// Initialize an empty arena
arena my_arena = {0};

// Allocate a struct in the arena
struct *my_struct = arena_alloc(&my_arena, sizeof *my_struct, ZEROMEM);

// Push an array onto the arena
int numbers[5] = {1, 2, 3, 4, 5};
int *arena_numbers = (int *)arena_push(&my_arena, numbers, sizeof numbers, ZEROMEM);

// Release all associated memory
arena_free(&my_arena, ZEROMEM);
```


## Notes

Glad is currently designed for Linux systems due to its reliance on mmap. The default chunk size and memory alignment settings are chosen to align with typical page sizes for x86 on Linux but can be modified to suit other environments.

## License

Glad is licensed under the Unlicense, making it fully public domain and free to use without restriction.
