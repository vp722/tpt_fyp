#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE_1GB (1UL << 30)  // 1GB
#define PAGE_SIZE 4096         // 4KB page
#define NUM_ACCESSES 100000    // Adjust for sufficient stress

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_1gb_regions>\n", argv[0]);
        return 1;
    }

    int num_regions = atoi(argv[1]);
    if (num_regions <= 0) {
        fprintf(stderr, "Invalid number of regions: %d\n", num_regions);
        return 1;
    }

    // Allocate N separate 1GB regions
    char **regions = malloc(num_regions * sizeof(char *));
    if (!regions) {
        perror("malloc");
        return 1;
    }

    // Map 1GB regions and touch one page in each
    for (int i = 0; i < num_regions; i++) {
        regions[i] = mmap(
            NULL,
            SIZE_1GB,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0
        );

        if (regions[i] == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        // Touch the first page to commit memory
        regions[i][0] = 0;
    }

    // Stress PDPTE cache with controlled accesses
    volatile char dummy;
    for (int iter = 0; iter < NUM_ACCESSES; iter++) {
        for (int i = 0; i < num_regions; i++) {
            dummy = regions[i][0];  // Read to force PDPTE access
        }
    }

    // Cleanup
    for (int i = 0; i < num_regions; i++) {
        munmap(regions[i], SIZE_1GB);
    }
    free(regions);

    return 0;
}