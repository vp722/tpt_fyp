#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SIZE_1GB (1UL << 30)  // 1GB
#define PAGE_SIZE 4096         // 4KB

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <buffer_size_GB> <num_accesses>\n", argv[0]);
        return 1;
    }

    size_t buffer_size_gb = atoi(argv[1]);
    size_t buffer_size = buffer_size_gb * SIZE_1GB;
    size_t num_accesses = atoi(argv[2]);

    // Allocate a large buffer
    char *buffer = mmap(
        NULL,
        buffer_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    );

    if (buffer == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Pre-touch all pages to populate page tables (optional but reduces noise)
    printf("Pre-touching memory...\n");
    for (size_t i = 0; i < buffer_size; i += PAGE_SIZE) {
        buffer[i] = 0;
    }

    // Generate random offsets across 1GB regions
    srand(time(NULL));
    volatile char dummy;
    printf("Stressing PDPTE cache with %zu GB buffer and %zu accesses...\n", buffer_size_gb, num_accesses);

    for (size_t i = 0; i < num_accesses; i++) {
        // Randomly select a 1GB "region" within the buffer
        size_t region = (size_t)rand() % buffer_size_gb;
        // Random offset within the selected 1GB region
        size_t offset = region * SIZE_1GB + (rand() % SIZE_1GB);
        dummy = buffer[offset];  // Access the memory
    }

    // Cleanup
    munmap(buffer, buffer_size);
    return 0;
}