#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#define BUFFER_SIZE (4ULL * 1024 * 1024 * 1024) // 4 GiB
#define ACCESS_GRANULARITY sizeof(uint64_t)
#define SLEEP_MICROSECONDS (30 * 1000000)      // 60 seconds
#define NUM_CYCLES 2

// Function to measure time in seconds
double get_time_in_seconds() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec + t.tv_usec / 1000000.0);
}

// Function to perform random memory accesses across the full buffer
void random_access_full(uint64_t *buffer, size_t num_elements) {
    uint64_t sum = 0;
    for (size_t i = 0; i < num_elements; i++) {
        size_t index = (((uint64_t)random() << 32) | random()) % num_elements;
        sum += buffer[index];
    }
    (void)sum; // Prevent compiler optimization
}

int main() {
    printf("Allocating %.2f GiB buffer...\n", BUFFER_SIZE / (1024.0 * 1024.0 * 1024.0));
    uint64_t *buffer = (uint64_t *)malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    size_t num_elements = BUFFER_SIZE / ACCESS_GRANULARITY;

    // Initialize buffer
    for (size_t i = 0; i < num_elements; i++) {
        buffer[i] = i;
    }

    printf("Starting %d workload-idle cycles...\n", NUM_CYCLES);
    double start = get_time_in_seconds();

    for (int cycle = 1; cycle <= NUM_CYCLES; cycle++) {

        printf("Cycle %d - Entering idle phase (sleep for 30 seconds)...\n", cycle);
        usleep(SLEEP_MICROSECONDS);

        printf("Cycle %d - Idle phase completed. Starting workload...\n", cycle);

        double workload_start = get_time_in_seconds();
        random_access_full(buffer, num_elements); // Access entire buffer randomly
        double workload_end = get_time_in_seconds();

        printf("Cycle %d - Workload time: %.2f seconds\n", cycle, workload_end - workload_start);

        
    }

    printf("Cycle %d - Entering idle phase (sleep for 30 seconds)...\n", cycle);
    usleep(SLEEP_MICROSECONDS);

    double end = get_time_in_seconds();
    printf("\nAll cycles completed.\n");
    printf("Total elapsed time (including idle): %.2f seconds\n", end - start);

    free(buffer);
    return EXIT_SUCCESS;
}
