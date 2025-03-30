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
#include <errno.h>  // Added missing header

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
                 uint64_t config, const char *name, pid_t pid) {
    memset(&counter->attr, 0, sizeof(counter->attr));
    counter->attr.type = type;
    counter->attr.size = sizeof(counter->attr);
    counter->attr.config = config;
    counter->attr.disabled = 1;
    counter->attr.inherit = 1;

    counter->fd = perf_event_open(&counter->attr, pid, -1, -1, 0);
    if (counter->fd < 0) {
        fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    counter->name = name;
}

void run_benchmark(const char *program, char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }
    
    struct perf_counter counters[3];
    pid_t pid = fork();
    
    if (pid == 0) { // Child process
        close(pipefd[1]);
        
        // Wait for parent's ready signal
        char dummy;
        ssize_t bytes_read = read(pipefd[0], &dummy, 1);
        close(pipefd[0]);
        
        if (bytes_read != 1) {
            fprintf(stderr, "Child failed to receive ready signal\n");
            exit(EXIT_FAILURE);
        }

        execvp(program, argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        close(pipefd[0]);
        
        // Initialize counters
        init_counter(&counters[0], PERF_TYPE_HARDWARE, 
                     PERF_COUNT_HW_CPU_CYCLES, "cycles", pid);
        init_counter(&counters[1], PERF_TYPE_HARDWARE,
                     PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid);
        init_counter(&counters[2], PERF_TYPE_SOFTWARE,
                     PERF_COUNT_SW_PAGE_FAULTS, "page-faults", pid);

        // Enable counters
        for (int i = 0; i < 3; i++) {
            if (ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
                fprintf(stderr, "Failed to enable %s: %s\n", 
                       counters[i].name, strerror(errno));
            }
        }

        // Signal child to start
        ssize_t bytes_written = write(pipefd[1], "G", 1);
        close(pipefd[1]);
        if (bytes_written != 1) {
            fprintf(stderr, "Failed to signal child process\n");
        }

        // Wait for benchmark completion
        int status;
        waitpid(pid, &status, 0);

        // Read and disable counters
        for (int i = 0; i < 3; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            ssize_t bytes_read = read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
            if (bytes_read != sizeof(uint64_t)) {
                fprintf(stderr, "Failed to read %s: %s\n",
                       counters[i].name, strerror(errno));
                counters[i].value = 0;
            }
            close(counters[i].fd);
        }

        // Print results
        printf("\nPerformance counters for '%s':\n", program);
        printf("%-20s: %'lu\n", "CPU Cycles", counters[0].value);
        printf("%-20s: %'lu\n", "Instructions", counters[1].value);
        printf("%-20s: %'lu\n", "Page Faults", counters[2].value);
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