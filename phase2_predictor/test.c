// this is a real time profiling tool - that collects metrics during the execution of the program
// and makes a decision to enable TPT (one way) for the rest of the execution
// this is a sampling based profiling tool extention for perf
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
#include <stdbool.h>
#include <time.h>
#include <syscall.h>

#include <inttypes.h> 

#define COUNTER_COUNT 2
#define SAMPLING_INTERVAL_MS 200 // 200ms

struct perf_counter {
    int fd;
    struct perf_event_attr attr;
    const char *name;
    uint64_t value;
    uint64_t prev_value;
    uint64_t delta;
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
    counter->attr.exclude_kernel = 1; 
    counter->attr.exclude_hv = 1;
    counter->attr.exclude_idle = 1;

    counter->fd = perf_event_open(&counter->attr, pid, -1, group_fd, 0);
    if (counter->fd < 0) {
        fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    counter->name = name;
    counter->value = 0;
    counter->prev_value = 0;
    counter->delta = 0;
}

void init_counters(struct perf_counter counters[], pid_t pid) {
    // Set cycles as group leader
    init_counter(&counters[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", pid, -1);

    init_counter(&counters[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid, -1);

//     init_counter(&counters[2], PERF_TYPE_RAW, 0x1008, "dtlb_load_misses_walk_duration", pid, -1);
//     init_counter(&counters[3], PERF_TYPE_RAW, 0x1049,"dtlb_store_misses_walk_duration", pid, -1);
//     init_counter(&counters[4], PERF_TYPE_RAW, 0x0e08, "dtlb_load_misses.walk_completed", pid, -1); 
//     init_counter(&counters[5], PERF_TYPE_RAW, 0x0e49, "dtlb_store_misses.walk_completed", pid, -1); 

//    init_counter(&counters[6], PERF_TYPE_RAW, 0x104f, "ept_walk_cycles", pid, -1);
}

void sample_counters(struct perf_counter counters[]) {
    for (int i = 0; i < COUNTER_COUNT; i++) {
        counters[i].prev_value = counters[i].value;
        ssize_t bytes_read = read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
        if (bytes_read != sizeof(uint64_t)) {
            fprintf(stderr, "In sasmple counters: Failed to read %s: %s\n", counters[i].name, strerror(errno));
            counters[i].value = 0;
        }

        // Calculate delta
        counters[i].delta = counters[i].value - counters[i].prev_value;
        printf("%s: %lu\n", counters[i].name, counters[i].value);
//	ioctl(counters[i].fd, PERF_EVENT_IOC_RESET, 0);
    }
}

void run_executable(const char *program, char *const argv[]) {
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
        read(pipefd[0], &dummy, 1); // blocking 
        close(pipefd[0]);

        execvp(program, argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // parent process; child pid 
        close(pipefd[0]);

        // open file 
        FILE *file = fopen("32m_rnd_write.csv", "w");
        if (!file) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        init_counters(counters, pid);

        for (int i = 0; i < COUNTER_COUNT; i++) {
            if (ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
                fprintf(stderr, "Failed to enable %s: %s\n", counters[i].name, strerror(errno));
            }
        }

        write(pipefd[1], "G", 1);
        close(pipefd[1]);

        int status;
        struct timespec last_sample_time;
        clock_gettime(CLOCK_MONOTONIC, &last_sample_time);

        // while the child process is running
        while (waitpid(pid, &status, WNOHANG) == 0) {
            struct timespec current_time;
            clock_gettime(CLOCK_MONOTONIC, &current_time);

            // Compute elapsed time in milliseconds
            long elapsed_ms = (current_time.tv_sec - last_sample_time.tv_sec) * 1000 +
                            (current_time.tv_nsec - last_sample_time.tv_nsec) / 1000000;

            if (elapsed_ms >= SAMPLING_INTERVAL_MS) {

                sample_counters(counters);

                last_sample_time = current_time;
            }

            // sleep for 10ms to reduce CPU usage while polling
            struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 10000000 }; // 10ms
            nanosleep(&sleep_time, NULL);
        }

        // Cleanup counters
        for (int i = 0; i < COUNTER_COUNT; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            close(counters[i].fd);
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Program execution failed.\n");
        }
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

    run_executable(argv[1], &argv[1]);
    return EXIT_SUCCESS;
}
