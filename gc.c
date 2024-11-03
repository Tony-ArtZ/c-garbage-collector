#include "gc.h"

// TODO: Unload registers to stack

static header_t empty = {0, NULL};
static header_t *free_list = &empty;
static header_t *used_list = NULL;
static uintptr_t stack_start;
static uintptr_t stack_end;

static int initialized = 0;

void print_lists()
{
  printf("\nUsed list:\n");
  for (header_t *curr = used_list; curr != NULL; curr = curr->next)
  {
    printf("Block at %p, size: %zu\n", (void *)curr, curr->size);
  }

  printf("\nFree list:\n");
  for (header_t *curr = free_list; curr != &empty; curr = curr->next)
  {
    printf("Block at %p, size: %zu\n", (void *)curr, curr->size);
  }
  printf("\n");
}

// Check if a pointer is valid (not null, 0 aligned, and within a reasonable range)
static inline int is_pointer_valid(void *ptr)
{
  uintptr_t p = (uintptr_t)ptr;
  return IS_ALIGNED(p) && p >= 0x1000 && p < 0x7fffffffffff;
}

// Set the LSB as marked in the next pointer as it is unused
static inline void set_marked(header_t *block)
{
  if (block && block->next)
  {
    block->next = (header_t *)((uintptr_t)(block->next) | MARKED_MASK);
  }
}

// Clear the marked bit
static inline void clear_marked(header_t *block)
{
  if (block && block->next)
  {
    block->next = (header_t *)((uintptr_t)(block->next) & ~MARKED_MASK);
  }
}

// Return 1 if the block is marked else 0
static inline int is_marked(header_t *block)
{
  return block && block->next && ((uintptr_t)(block->next) & MARKED_MASK);
}

// Get the actual pointer from the next pointer by using doing bitwise AND with the inverted mask
static inline header_t *get_actual_pointer(header_t *block)
{
  return block ? (header_t *)((uintptr_t)(block->next) & ~MARKED_MASK) : NULL;
}

// Helper function to check if two blocks are adjacent
static inline int is_adjacent(header_t *first, header_t *second)
{
  // Check if the end of the first block is the same as the start of the second block
  return (char *)first + (first->size * sizeof(header_t)) == (char *)second;
}

static void add_to_free_list(header_t *block)
{
  if (!block)
    return;

  // Initialize the block
  block->next = NULL;

  // If free list is empty, add the block
  if (free_list == &empty)
  {
    free_list = block;
    block->next = &empty;
    return;
  }

  header_t *curr = free_list;
  header_t *prev = NULL;

  // Find appropriate position by looking at the address
  while (curr != &empty && curr < block)
  {
    prev = curr;
    curr = curr->next;
  }

  // Case 1: Insert at the beginning by coallescing with the previous block if possible
  if (prev && is_adjacent(prev, block))
  {
    prev->size += block->size;
    block = prev;
  }
  else if (prev)
  {
    prev->next = block;
  }
  else
  {
    free_list = block;
  }

  // Case 2: Try to coalesce with next block
  if (curr != &empty && is_adjacent(block, curr))
  {
    block->size += curr->size;
    block->next = curr->next;
  }
  else
  {
    block->next = curr;
  }

  printf("Coalescing complete - block at %p with size %zu\n", (void *)block, block->size);
}

// Split a block into two if the remainder is large enough
static header_t *split_block(header_t *block, size_t size)
{
  if (!block || block->size <= size)
    return block;

  // Only split if the remainder is large enough
  if (block->size - size > 1)
  {
    header_t *new_block = (header_t *)((char *)block + size * sizeof(header_t));
    new_block->size = block->size - size;
    block->size = size;

    printf("Split block: original at %p (size %zu), new at %p (size %zu)\n",
           (void *)block, block->size, (void *)new_block, new_block->size);

    return new_block;
  }
  return NULL;
}

// Uses sbrk to allocate more memory (can cause conflicts with malloc/calloc) *can be replaced with mmap for larger allocations)
static header_t *get_more_memory(size_t size)
{
  size_t alloc_size = size * sizeof(header_t);
  if (alloc_size < MIN_PAGE_SIZE)
  {
    alloc_size = MIN_PAGE_SIZE;
  }

  header_t *block = sbrk(alloc_size);
  if (block == (void *)-1)
  {
    return NULL;
  }

  block->size = alloc_size / sizeof(header_t);
  block->next = NULL;

  printf("Allocated new memory block at %p, size %zu\n", (void *)block, block->size);
  return block;
}

void *gc_alloc(size_t size)
{
  if (size == 0)
    return NULL;

  // Calculate required size including header
  size_t total_size = (size + sizeof(header_t) - 1) / sizeof(header_t) + 1;

  // Find a free block
  header_t *curr = free_list;
  header_t *prev = NULL;

  while (curr != &empty && curr->size < total_size)
  {
    prev = curr;
    curr = curr->next;
  }

  // If no suitable block found, get more memory
  if (curr == &empty)
  {
    curr = get_more_memory(total_size);
    if (!curr)
      return NULL;
    add_to_free_list(curr);
    prev = NULL;
    curr = free_list; // Start search again with new block
  }

  // Remove from free list
  if (prev)
  {
    prev->next = curr->next;
  }
  else
  {
    free_list = curr->next;
  }

  // Split block if it's too large
  header_t *remainder = split_block(curr, total_size);
  if (remainder)
  {
    add_to_free_list(remainder);
  }

  // Add to used list
  curr->next = used_list;
  used_list = curr;

  printf("Allocated block at %p, size %zu\n", (void *)curr, curr->size);

  return (void *)(curr + 1);
}

// Scan the stack range for pointers to the heap and mark them
static void scan_range(void *start, void *end)
{
  if (!start || !end || start >= end)
    return;

  uintptr_t *curr = (uintptr_t *)start;
  const uintptr_t *limit = (uintptr_t *)end;

  while (curr < limit)
  {
    if (is_pointer_valid((void *)*curr))
    {
      for (header_t *block = used_list; block != NULL; block = get_actual_pointer(block))
      {
        // Check if the pointer is within the block
        void *block_start = (void *)(block + 1);
        void *block_end = (void *)((char *)block_start + (block->size - 1) * sizeof(header_t));

        if ((void *)*curr >= block_start && (void *)*curr < block_end)
        {
          // Mark the block
          set_marked(block);
          break;
        }
      }
    }
    curr++;
  }
}

// Scan the heap for pointers to other blocks and mark them
static void scan_heap()
{
  for (header_t *block = used_list; block != NULL; block = get_actual_pointer(block))
  {
    void *block_start = (void *)(block + 1);
    void *block_end = (void *)((char *)block_start + (block->size - 1) * sizeof(header_t));
    scan_range(block_start, block_end);
  }
}

// Mark and sweep garbage collection
void gc()
{
  if (!initialized)
  {
    initialized = 1;
    get_stack_pointer();
  }

  printf("\nStarting garbage collection...\n");

  if (stack_start == 0 || stack_end == 0 || stack_start >= stack_end)
  {
    printf("Invalid stack pointers, skipping GC\n");
    return;
  }

  // Clear all marks first
  for (header_t *block = used_list; block != NULL; block = get_actual_pointer(block))
  {
    clear_marked(block);
  }

  printf("Scanning stack range: %p to %p\n", (void *)stack_start, (void *)stack_end);
  scan_range((void *)stack_start, (void *)stack_end);

  printf("Scanning heap for pointers...\n");
  scan_heap();

  // Sweep phase
  header_t *curr = used_list;
  header_t *prev = NULL;

  while (curr != NULL)
  {
    header_t *next = get_actual_pointer(curr);

    // If the block is not marked, free it
    if (!is_marked(curr))
    {
      if (prev == NULL)
      {
        used_list = next;
      }
      else
      {
        prev->next = next;
      }

      header_t *to_free = curr;
      curr = next;

      printf("Collecting block at %p (size %zu)\n", (void *)to_free, to_free->size);
      add_to_free_list(to_free);
    }
    else
    {
      clear_marked(curr);
      prev = curr;
      curr = next;
    }
  }

  printf("Garbage collection completed\n");
}

// Get the current stack pointer and stack range using inline assembly and pthread functions
void get_stack_pointer()
{
  void *stack_ptr;

#if defined(__x86_64__)
  asm volatile("movq %%rsp, %0" : "=r"(stack_ptr));
#elif defined(__i386__)
  asm volatile("movl %%esp, %0" : "=r"(stack_ptr));
#endif

  pthread_attr_t attrs;
  if (pthread_getattr_np(pthread_self(), &attrs) != 0)
  {
    fprintf(stderr, "Failed to get thread attributes\n");
    return;
  }

  void *stack_addr;
  size_t stack_size;
  if (pthread_attr_getstack(&attrs, &stack_addr, &stack_size) != 0)
  {
    fprintf(stderr, "Failed to get stack information\n");
    pthread_attr_destroy(&attrs);
    return;
  }

  stack_start = (uintptr_t)stack_ptr;
  stack_end = (uintptr_t)stack_addr + stack_size;

  pthread_attr_destroy(&attrs);

  printf("Current stack pointer: %p\n", stack_ptr);
  printf("Stack base address: %p\n", stack_addr);
  printf("Stack top address: %p\n", (void *)((char *)stack_addr + stack_size));
  printf("Stack size: %zu bytes\n", stack_size);
}

void gc_destroy()
{
  header_t *curr = used_list;
  while (curr != NULL)
  {
    header_t *next = get_actual_pointer(curr);
    printf("Freeing block at %p (size %zu)\n", (void *)curr, curr->size);
    curr = next;
  }
  used_list = NULL;

  curr = free_list;
  while (curr != &empty)
  {
    header_t *next = curr->next;
    printf("Freeing block at %p (size %zu)\n", (void *)curr, curr->size);
    curr = next;
  }
  free_list = &empty;

  printf("All memory freed\n");
}