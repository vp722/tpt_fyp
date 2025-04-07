#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define COUNTER_COUNT 8

struct perf_counter {
    int fd;
    struct perf_event_attr attr;
    const char *name;
    uint64_t value;
};

static int perf_event_open(struct perf_event_attr *attr, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

void init_counter(struct perf_counter *counter, uint32_t type, 
                uint64_t config, const char *name, pid_t pid, int group_fd) {
    memset(&counter->attr, 0, sizeof(counter->attr));
    counter->attr.type = type;
    counter->attr.size = sizeof(counter->attr);
    counter->attr.config = config;
    counter->attr.disabled = 1;
    counter->attr.inherit = 1;

    // Attach to the group leader
    counter->fd = perf_event_open(&counter->attr, pid, -1, group_fd, 0);
    if (counter->fd < 0) {
    fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
    exit(EXIT_FAILURE);
    }
    counter->name = name;
}

void init_counters(struct perf_counter counters[], pid_t pid) {
    init_counter(&counters[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", pid, -1);
    init_counter(&counters[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid, -1);
    init_counter(&counters[2], PERF_TYPE_RAW, 0x1008, "dtlb_load_misses_walk_duration", pid, -1);
    init_counter(&counters[3], PERF_TYPE_RAW, 0x1049,"dtlb_store_misses_walk_duration", pid, -1);
    // init_counter(&counters[4], PERF_TYPE_RAW, 0x1085, "itlb_misses_walk_duration", pid);
    // init_counter(&counters[5], PERF_TYPE_RAW, 0x108, "dtlb_load_misses.miss_causes_a_walk", pid);
    init_counter(&counters[4], PERF_TYPE_RAW, 0xe08, "dtlb_load_misses.walk_completed", pid, -1); 
    init_counter(&counters[5], PERF_TYPE_RAW, 0x149, "dtlb_store_misses.miss_causes_a_walk", pid, -1);
//     init_counter(&counters[8], PERF_TYPE_RAW, 0xe49, "dtlb_store_misses.walk_completed", pid); 
    init_counter(&counters[6], PERF_TYPE_RAW, 0x185, "itlb_misses.miss_causes_a_walk", pid, -1);
    init_counter(&counters[7], PERF_TYPE_RAW, 0xe85, "itlb_misses.walk_completed", pid, -1);
}

void run_benchmark(const char *program, char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }

    struct perf_counter counters[COUNTER_COUNT];
    pid_t pid = fork();

    if (pid == 0) {
        close(pipefd[1]);
        char dummy;
        ssize_t bytes_read = read(pipefd[0], &dummy, 1);
        close(pipefd[0]);

        if (bytes_read != 1) {
            fprintf(stderr, "Child failed to receive ready signal\n");
            exit(EXIT_FAILURE);
        }

        if (execvp(program, argv) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);  // Ensure the child exits immediately
        }
        
    } else if (pid > 0) {
        close(pipefd[0]);
        
        init_counters(counters, pid);

        // enable perforamnce counters
        for (int i = 0; i < COUNTER_COUNT; i++) {
            if (ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
                fprintf(stderr, "Failed to enable %s: %s\n", counters[i].name, strerror(errno));
            }
        }

        // signal the child process to start
        ssize_t bytes_written = write(pipefd[1], "G", 1);
        close(pipefd[1]);
        if (bytes_written != 1) {
            fprintf(stderr, "Failed to signal child process\n");
        }

        int status;
        waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Program execution failed. Discarding performance results.\n");
            return;
        }

        for (int i = 0; i < COUNTER_COUNT; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            ssize_t bytes_read = read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
            if (bytes_read != sizeof(uint64_t)) {
                fprintf(stderr, "Failed to read %s: %s\n", counters[i].name, strerror(errno));
                counters[i].value = 0;
            }
            close(counters[i].fd);
        }

        // uint64_t total_page_walk_cycles = counters[2].value + counters[3].value + counters[4].value;   

        printf("\nPerformance counters for '%s':\n", program);
        printf("%-20s: %'lu\n", "CPU Cycles", counters[0].value);
        printf("%-20s: %'lu\n", "Instructions", counters[1].value);
        // printf("\nPage Walk Analysis:\n");
        printf("%-20s: %'lu\n", "DTLB Load Walk Cycles", counters[2].value);
        printf("%-20s: %'lu\n", "DTLB Store Walk Cycles", counters[3].value);
        // printf("%-20s: %'lu\n", "ITLB Walk Cycles", counters[4].value);
        // printf("%-20s: %'lu\n", "Total Page Walk Cycles", total_page_walk_cycles);

        printf("\nDTLB Load Misses:\n");
        // printf("%-20s: %'lu\n", "Miss Causes a Walk", counters[5].value);
        printf("%-20s: %'lu\n", "Walk Completed", counters[4].value);
        // printf("\nDTLB Store Misses:\n");
        printf("%-20s: %'lu\n", "Miss Causes a Walk", counters[5].value);
        // printf("%-20s: %'lu\n", "Walk Completed", counters[8].value);
        // printf("\nITLB Misses:\n");
        printf("%-20s: %'lu\n", "Miss Causes a Walk", counters[6].value);
        printf("%-20s: %'lu\n", "Walk Completed", counters[7].value);
        // printf("\n");



    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    run_benchmark(argv[1], &argv[1]);
    return EXIT_SUCCESS;
}