#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define BUFFER_SIZE (1ULL * 1024 * 1024 * 1024) // 10 GiB
#define ACCESS_GRANULARITY sizeof(uint64_t) // 64-bit (8 bytes)

// Function to perform sequential memory access
void sequential_access(uint64_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint64_t value = buffer[i]; // Read 64-bit value
        (void)value; // Prevent compiler optimization
    }
}

// Function to perform random memory access
void random_access(uint64_t *buffer, size_t size) {
    size_t num_accesses = size;
    for (size_t i = 0; i < num_accesses; i++) {
        size_t index = rand() % num_accesses; // Random index
        uint64_t value = buffer[index]; // Read 64-bit value
        (void)value; // Prevent compiler optimization
    }
}

int main() {
    // Allocate 1 GiB buffer
    uint64_t *buffer = (uint64_t *)malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    // Initialize buffer with some data
    for (size_t i = 0; i < BUFFER_SIZE / sizeof(uint64_t); i++) {
        buffer[i] = i;
    }

    // Measure sequential access time
    clock_t start = clock();
    sequential_access(buffer, BUFFER_SIZE / sizeof(uint64_t));
    clock_t end = clock();
    double sequential_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Sequential access time: %.2f seconds\n", sequential_time);

    // Measure random access time
    start = clock();
    random_access(buffer, BUFFER_SIZE / sizeof(uint64_t));
    end = clock();
    double random_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Random access time: %.2f seconds\n", random_time);

    // Free the buffer
    free(buffer);

    return EXIT_SUCCESS;
}
