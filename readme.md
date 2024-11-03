# C Garbage Collector

This project implements a stop-the-world conservative garbage collector in C. The garbage collector uses a mark-and-sweep algorithm to manage memory allocation and deallocation.

## Warning

This project uses the `sbrk` system call for memory management, which may not be compatible with the `malloc` function provided by the standard C library. Using `sbrk` and `malloc` together can lead to undefined behavior and memory corruption. It is recommended to avoid using `malloc` in conjunction with this garbage collector.

## Algorithm

### Stop-the-World Conservative GC

The garbage collector is a stop-the-world type, meaning that it halts the execution of the program to perform garbage collection. It is also conservative, which means it does not require precise information about the locations of all pointers. Instead, it scans memory regions that might contain pointers and treats any value that looks like a pointer as a potential reference to a heap object. It uses an `explicit free list` along with `first fit` allocation strategy to manage memory.


### Mark-and-Sweep Algorithm

1. **Mark Phase**: The collector scans the stack and heap to find all reachable objects. It marks these objects to indicate that they are still in use.
2. **Sweep Phase**: The collector scans the heap again and collects all unmarked objects, returning their memory to the free list.

## How to Run

1. **Compile and Run the Project**:
   ```sh
   make 
2. ** Clean the Project**:
   ```sh
   make clean
The ```Makefile``` provided in the project handles the compilation and execution of the program. The main program (```main.c```) includes a test function to demonstrate the garbage collector's functionality.

## Files
    1. main.c: Main program that demonstrates the garbage collector.
    2. gc.c: Garbage collector implementation.
    3. gc.h: Header file for the garbage collector.
    4. Makefile: Makefile for compiling the project.
    5. README.md: Project documentation.

    ## Installation

    To install the project, clone the repository and navigate to the project directory:
    ```sh
    git clone https://github.com/yourusername/c-gc.git
    cd c-gc
    ```

    ## Usage

    To use the garbage collector in your own project, include `gc.h` and link against `gc.c`. Here is an example of how to integrate the garbage collector:

    ```c
    #include "gc.h"

    int main() {
        int *allocated = gc_alloc(sizeof(int));
        gc(); // Run garbage collection
        gc_destroy();
        return 0;
    }
    ```

    ## Contributing

    Contributions are welcome! Please fork the repository and submit a pull request with your changes. Ensure that your code follows the project's coding standards.

