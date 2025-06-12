// This is a simple C program that simulates 
// workload-idle cycles with random memory accesses.
// Similar to memcached work flow but the idle phase is much simpler

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>  // For usleep()

#define BUFFER_SIZE (4ULL * 1024 * 1024 * 1024) // 4 GiB buffer
#define NUM_ACCESSES (10000000ULL)              // Number of memory accesses per cycle
#define ACCESS_GRANULARITY sizeof(uint64_t)    // 64-bit (8 bytes)
#define SLEEP_MICROSECONDS (60 * 1000000)      // 60 seconds sleep (idle phase)
#define NUM_CYCLES 3                           // Number of workload-idle cycles

// Function to measure time in seconds
double get_time_in_seconds() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec + t.tv_usec / 1000000.0);
}

// Function to perform random memory accesses
void random_access(uint64_t *buffer, size_t size) {
    for (size_t i = 0; i < NUM_ACCESSES; i++) {
        size_t index = (((uint64_t)random() << 32) | random()) % size;
        uint64_t value = buffer[index];
        (void)value; // Prevent compiler optimization
    }
}

int main() {
    printf("Allocating %.2f GiB buffer...\n", BUFFER_SIZE / (1024.0 * 1024.0 * 1024.0));
    uint64_t *buffer = (uint64_t *)malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    size_t num_elements = BUFFER_SIZE / sizeof(uint64_t);

    // Initialize buffer
    for (size_t i = 0; i < num_elements; i++) {
        buffer[i] = i;
    }

    printf("Starting %d workload-idle cycles...\n", NUM_CYCLES);
    double start = get_time_in_seconds();

    for (int cycle = 1; cycle <= NUM_CYCLES; cycle++) {
        printf("\n--- Cycle %d ---\n", cycle);

        double workload_start = get_time_in_seconds();
        random_access(buffer, num_elements);
        double workload_end = get_time_in_seconds();

        printf("Cycle %d - Workload time: %.2f seconds\n", cycle, workload_end - workload_start);

        printf("Cycle %d - Entering idle phase (sleep for 60 seconds)...\n", cycle);
        usleep(SLEEP_MICROSECONDS);
    }

    double end = get_time_in_seconds();
    printf("\nAll cycles completed.\n");
    printf("Total elapsed time (including idle): %.2f seconds\n", end - start);

    free(buffer);
    return EXIT_SUCCESS;
}
