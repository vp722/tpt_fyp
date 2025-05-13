// rt_profiler_group.c
// This is a real time profiling tool that collects metrics during the execution
// of a target program and (via a heuristic) makes a decision to enable TPT for
// the rest of the execution.
// All performance counters are grouped under one leader (cycles) to reduce overhead.

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

#define COUNTER_COUNT 7
#define SAMPLING_INTERVAL_SEC 1
#define AVG_WALK_CYCLES 25  // Threshold average cycles

// Structure holding info about a counter.
struct perf_counter {
    int fd;
    struct perf_event_attr attr;
    const char *name;
    uint64_t prev_value;
    uint64_t delta;
};

// perf_event_open wrapper.
static int perf_event_open(struct perf_event_attr *attr, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

// Initialize one counter. When grouping, set read_format to PERF_FORMAT_GROUP.
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
    // Set group read format so we can read all counters at once
    counter->attr.read_format = PERF_FORMAT_GROUP;

    counter->fd = perf_event_open(&counter->attr, pid, -1, group_fd, 0);
    if (counter->fd < 0) {
        fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    counter->name = name;
    counter->prev_value = 0;
    counter->delta = 0;
}

// Initialize all counters in a group; cycles is the leader.
void init_counters(struct perf_counter counters[], pid_t pid) {
    // Initialize group leader "cycles"
    init_counter(&counters[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", pid, -1);
    int group_fd = counters[0].fd;
    // Open the other counters using group_fd so they're part of the same group.
    init_counter(&counters[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid, group_fd);
    init_counter(&counters[2], PERF_TYPE_RAW, 0x1008, "dtlb_load_misses_walk_duration", pid, group_fd);
    init_counter(&counters[3], PERF_TYPE_RAW, 0x1049, "dtlb_store_misses_walk_duration", pid, group_fd);
    init_counter(&counters[4], PERF_TYPE_RAW, 0x0e08, "dtlb_load_misses.walk_completed", pid, group_fd);
    init_counter(&counters[5], PERF_TYPE_RAW, 0x0e49, "dtlb_store_misses.walk_completed", pid, group_fd);
    init_counter(&counters[6], PERF_TYPE_RAW, 0x104f, "ept_walk_cycles", pid, group_fd);
}

// Return the resident set size (RSS) in bytes for a given pid.
uint64_t get_rss_in_bytes(pid_t pid) {
    char filename[256];
    snprintf(filename, sizeof(filename), "/proc/%d/statm", pid);
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }

    uint64_t rss_value;
    if (fscanf(file, "%*s %lu", &rss_value) != 1) {
        perror("fscanf");
        fclose(file);
        return 0;
    }
    fclose(file);
    // Convert from pages to bytes.
    return rss_value * getpagesize();
}

// Sample counters from the group leader. This function reads all counters at once
// into a buffer using PERF_FORMAT_GROUP. The layout is: [num_events, val0, val1, ..., valN].
void sample_counters_group(struct perf_counter counters[]) {
    uint64_t buf[COUNTER_COUNT + 1];  // First element is count; then one value per counter.
    ssize_t bytes_read = read(counters[0].fd, buf, sizeof(buf));
    if (bytes_read != sizeof(buf)) {
        fprintf(stderr, "Group read failed: %s\n", strerror(errno));
        return;
    }
    // buf[0] is the count of events.
    for (int i = 0; i < COUNTER_COUNT; i++) {
        uint64_t new_value = buf[i+1];
        counters[i].delta = new_value - counters[i].prev_value;
        counters[i].prev_value = new_value;
    }
}

// Heuristic: if the average ept_walk_cycles (per completed walk) is greater than threshold,
// and resident set size (RSS) is ≥ 1 GiB, return 1 (enable TPT); otherwise, 0.
int should_enable_tpt(struct perf_counter counters[], pid_t pid) {
    // We expect counters[4] and counters[5] to give completed walks, and counters[6] is ept_walk_cycles.
    uint64_t walks_completed = counters[4].delta + counters[5].delta;
    double avg_ept_walk_cycles = (walks_completed > 0) ? (double)counters[6].delta / walks_completed : 0.0;
    uint64_t rss = get_rss_in_bytes(pid);
    double rss_in_gb = (double)rss / (1024 * 1024 * 1024);

    printf("ept_walk_cycles delta: %lu\n", counters[6].delta);
    printf("walks_completed delta: %lu\n", walks_completed);
    printf("avg_ept_walk_cycles: %.2lf\n", avg_ept_walk_cycles);
    printf("RSS: %.2lf GB\n", rss_in_gb);

    // A small error margin (e.g., 5%) can be applied.
    double error_margin = 0.05;
    double lower_bound = AVG_WALK_CYCLES * (1.0 - error_margin);
    // For example, we check if the avg cycles is above the lower bound.
    bool in_range = avg_ept_walk_cycles > lower_bound;

    if (rss_in_gb >= 1.0 && in_range) {
        return 1;  // enable TPT
    }
    return 0; // do not enable TPT
}

// Run a target executable while collecting performance metrics.
// The parent's sampling loop will decide (based on the counters) whether to "enable TPT."
void run_executable(const char *program, char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }

    struct perf_counter counters[COUNTER_COUNT];
    pid_t pid = fork();

    if (pid == 0) {
        // Child process: block until parent signals to start.
        close(pipefd[1]);
        char dummy;
        read(pipefd[0], &dummy, 1); // block until signal
        close(pipefd[0]);
        execvp(program, argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process: set up counters, then signal child.
        close(pipefd[0]);
        init_counters(counters, pid);

        // Enable counters through the leader. Since they are grouped, this enables all.
        if (ioctl(counters[0].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
            fprintf(stderr, "Failed to enable group leader: %s\n", strerror(errno));
        }
        // Signal child to start execution.
        write(pipefd[1], "G", 1);
        close(pipefd[1]);

        int status;
        time_t last_sample_time = time(NULL);
        bool enabled_tpt = false;

        // Sampling loop while child is running.
        while (waitpid(pid, &status, WNOHANG) == 0) {
            time_t current_time = time(NULL);
            if (!enabled_tpt && (current_time - last_sample_time >= SAMPLING_INTERVAL_SEC)) {
                sample_counters_group(counters);
                if (should_enable_tpt(counters, pid)) {
                    printf("=========== ACTION : ENABLE TPT ===========\n");
                    enabled_tpt = true;
                    // Here you can add code (e.g., hypercall, procfs write) to actually enable TPT.
                }
                last_sample_time = current_time;
            }
            struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 100000000 }; // sleep 100ms
            nanosleep(&sleep_time, NULL);
        }

        // Disable counters via group leader.
        ioctl(counters[0].fd, PERF_EVENT_IOC_DISABLE, 0);
        // Close all counter file descriptors.
        for (int i = 0; i < COUNTER_COUNT; i++) {
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
