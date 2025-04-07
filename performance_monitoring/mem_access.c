#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>  // For gettimeofday()

#define BUFFER_SIZE (16ULL * 1024 * 1024 * 1024) // 8 GiB
#define NUM_ACCESSES (1ULL * 1024 * 1024 * 1024)
#define ACCESS_GRANULARITY sizeof(uint64_t) // 64-bit (8 bytes)

// Function to measure time in seconds
double get_time_in_seconds() {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    return (start_time.tv_sec + start_time.tv_usec / 1000000.0);
}

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
    for (size_t i = 0; i < NUM_ACCESSES; i++) {
        size_t index = (((uint64_t)random() << 32) | random()) % num_accesses; // Random index
        uint64_t value = buffer[index]; // Read 64-bit value
        (void)value; // Prevent compiler optimization
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <access_type>\n", argv[0]);
        fprintf(stderr, "<access_type> should be 'sequential' or 'random'\n");
        return EXIT_FAILURE;
    }

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

    double start_time, end_time, access_time;

    // Determine which type of access to perform based on the argument
    if (strcmp(argv[1], "sequential") == 0) {
        start_time = get_time_in_seconds();
        sequential_access(buffer, BUFFER_SIZE / sizeof(uint64_t));
        end_time = get_time_in_seconds();
        access_time = end_time - start_time;
        printf("Sequential access time: %.2f seconds\n", access_time);
    } 
    else if (strcmp(argv[1], "random") == 0) {
        start_time = get_time_in_seconds();
        random_access(buffer, BUFFER_SIZE / sizeof(uint64_t));
        end_time = get_time_in_seconds();
        access_time = end_time - start_time;
        printf("Random access time: %.2f seconds\n", access_time);
    } 
    else {
        fprintf(stderr, "Invalid argument: %s. Please use 'sequential' or 'random'.\n", argv[1]);
        free(buffer);
        return EXIT_FAILURE;
    }

    // Free the buffer
    free(buffer);

    return EXIT_SUCCESS;
}

