# Glad - An Arena Allocator

Glad is a small, single header arena allocator backed by `mmap` for use on UNIX systems.

## Usage
To use Glad in your project, clone the repository and include the header `glad.h` in your code. Compile your project with any C99-compliant compiler. Its main usecase is in handling data with tightly coupled lifetimes, that may be inconvenient to handle with `malloc`/`free`. Throw a tree on the arena, and throw it all away when you're done!


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

Glad is currently designed for Linux systems due to its reliance on mmap. The default chunk size and memory alignment settings are chosen to align with typical page sizes for x86 on Linux but can be modified to suit other environments.

## License

Glad is licensed under the Unlicense, making it fully public domain and free to use without restriction.
