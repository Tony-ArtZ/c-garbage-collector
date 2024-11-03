#ifndef GC_H
#define GC_H
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef struct header
{
    size_t size;
    struct header *next;
} header_t;

#define MARKED_MASK 0x1
#define IS_ALIGNED(p) ((uintptr_t)(p) % sizeof(void *) == 0)
#define MARKED_MASK 0x1
#define MIN_PAGE_SIZE 4096

#define GC_THRESHOLD 10

void get_stack_pointer();
void *gc_alloc(size_t size);
void gc();
void print_lists();
void gc_destroy();

#endif