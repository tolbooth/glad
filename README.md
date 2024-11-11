# glad - An Arena Allocator

glad is a small, single header arena allocator backed by `mmap` for use on UNIX systems.

## Usage
To use glad in your project, just include the header `glad.h` in your code. The helper macros make use of `alignof`, so they rely on C2X or compiler extensions -- if you don't care to use them, any C99-compliant compiler will work! 

The main usecase of glad is in handling data with tightly coupled lifetimes whose size is not known at runtime. Trees, Linked Lists, Hash Maps, etc. may be inconvenient to handle with `malloc`/`free`: throw them on the arena, and throw it all away when you're done! 

`glad_new` and `glad_push` are simple variadic macros to help make basic usage a little less cluttered. You can manually specify alignment via direct calls to `arena_alloc` and `arena_push`.


```c
#include "glad.h"

typedef struct {
    int id;
    char name[64];
} Person;

arena ar = {0}; // Initialize an empty arena

// Allocate a single Person using glad_new
Person* person = glad_new(&ar, Person); 
person->id = 1;
strcpy(person->name, "John Doe");

// Allocate an array of 10 Persons using glad_new
Person* people = glad_new(&ar, Person, 10);

// Push data into the arena using glad_push
int data[128] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, ...};
int* pushed_data = glad_push(&ar, int, data, 128);

arena_free(&ar, ZEROMEM);
```


## Notes

glad is currently designed for Linux systems due to its reliance on mmap. The default chunk size and memory alignment settings are chosen to align with typical page sizes for x86 on Linux but can be modified to suit other environments.

The main goal of an arena allocator is to simplify the logic of handling dynamically allocated structures. In testing, I've found a significant performance gain over `malloc` when performing consecutive small allocations. This graph was generated via callgrind and [gprof2dot](https://github.com/jrfonseca/gprof2dot).

![Call graph](images/call_graph.png?raw=true)

## License

glad is licensed under the Unlicense, making it fully public domain and free to use without restriction.
