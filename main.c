#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "gc.h"

void test_gc()
{
    printf("\n=== Starting GC Test ===\n");

    // Test 1: Basic allocation and collection
    printf("\nTest 1: Basic allocation and collection\n");
    int *will_survive = gc_alloc(sizeof(int));
    if (!will_survive)
    {
        printf("Failed to allocate will_survive\n");
        return;
    }

    *will_survive = 4;
    printf("Allocated will_survive with value %d at %p\n", *will_survive, (void *)will_survive);

    // Create some garbage with different sizes to test coalescing
    printf("Creating garbage allocations...\n");
    for (int i = 0; i < 20; i++)
    {
        int *temp = gc_alloc(sizeof(int) * (i + 1)); // Varying sizes
        printf("WILL SURVIVE: %d\n", *will_survive);
        if (temp)
            *temp = i;
    }

    printf("Before GC: will_survive = %d\n", *will_survive);
    print_lists();

    gc();

    printf("After GC: will_survive = %d\n", *will_survive);
    print_lists();

    printf("=== GC Test Completed ===\n\n");
    gc_destroy();
}

int main()
{
    test_gc();
    return 0;
}